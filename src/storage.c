#include "header.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ATM_NO_SQLITE
#include <sqlite3.h>
#endif

#ifdef _WIN32
#include <direct.h>
#define ATM_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define ATM_MKDIR(path) mkdir((path), 0755)
#endif

static const char *data_dir(void) {
    const char *custom = getenv("ATM_DATA_DIR");
    return (custom != NULL && *custom != '\0') ? custom : "data";
}

static bool build_path(char output[ATM_PATH_LEN], const char *name) {
    int written = snprintf(output, ATM_PATH_LEN, "%s/%s", data_dir(), name);
    return written > 0 && written < ATM_PATH_LEN;
}

static bool touch_if_missing(const char *path) {
    FILE *file = fopen(path, "a");
    if (file == NULL) return false;
    return fclose(file) == 0;
}

bool storage_uses_sqlite(void) {
#ifdef ATM_NO_SQLITE
    return false;
#else
    const char *requested = getenv("ATM_STORAGE");
    return requested == NULL || *requested == '\0' || strcmp(requested, "text") != 0;
#endif
}

const char *storage_backend_name(void) {
    return storage_uses_sqlite() ? "SQLite" : "text files";
}

static bool text_load_users(User *users, size_t capacity, size_t *count) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "users.txt")) return false;
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Unable to open users storage");
        return false;
    }
    *count = 0U;
    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (*count >= capacity) {
            fputs("Too many users in storage.\n", stderr);
            fclose(file);
            return false;
        }
        User user;
        if (sscanf(line, "%d %63s %127s", &user.id, user.name, user.password) == 3) users[(*count)++] = user;
    }
    bool ok = !ferror(file);
    fclose(file);
    return ok;
}

static bool text_save_users(const User *users, size_t count) {
    char path[ATM_PATH_LEN], temp[ATM_PATH_LEN];
    if (!build_path(path, "users.txt") || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) return false;
    FILE *file = fopen(temp, "w");
    if (file == NULL) {
        perror("Unable to write users storage");
        return false;
    }
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        if (fprintf(file, "%d %s %s\n", users[i].id, users[i].name, users[i].password) < 0) { ok = false; break; }
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) { remove(temp); return false; }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(temp, path) != 0) {
        perror("Unable to replace users storage");
        remove(temp);
        return false;
    }
    return true;
}

static bool text_load_accounts(Account *accounts, size_t capacity, size_t *count) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "records.txt")) return false;
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Unable to open account storage");
        return false;
    }
    *count = 0U;
    char line[768];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (*count >= capacity) {
            fputs("Too many accounts in storage.\n", stderr);
            fclose(file);
            return false;
        }
        Account account;
        char date_text[32], type_text[ATM_TYPE_LEN];
        int parsed = sscanf(line, "%d %d %63s %lld %31s %63s %31s %lf %15s",
                            &account.id, &account.user_id, account.owner, &account.number,
                            date_text, account.country, account.phone, &account.balance, type_text);
        if (parsed == 9 && parse_date(date_text, &account.created)) {
            account.type = account_type_from_string(type_text);
            if (account.type != ACCOUNT_INVALID) accounts[(*count)++] = account;
        }
    }
    bool ok = !ferror(file);
    fclose(file);
    return ok;
}

static bool text_save_accounts(const Account *accounts, size_t count) {
    char path[ATM_PATH_LEN], temp[ATM_PATH_LEN];
    if (!build_path(path, "records.txt") || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) return false;
    FILE *file = fopen(temp, "w");
    if (file == NULL) {
        perror("Unable to write account storage");
        return false;
    }
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        const Account *a = &accounts[i];
        if (fprintf(file, "%d %d %s %lld %02d/%02d/%04d %s %s %.2f %s\n",
                    a->id, a->user_id, a->owner, a->number,
                    a->created.day, a->created.month, a->created.year,
                    a->country, a->phone, a->balance, account_type_name(a->type)) < 0) { ok = false; break; }
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) { remove(temp); return false; }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(temp, path) != 0) {
        perror("Unable to replace account storage");
        remove(temp);
        return false;
    }
    return true;
}

#ifndef ATM_NO_SQLITE
static bool sqlite_exec(sqlite3 *db, const char *sql) {
    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\n", error != NULL ? error : sqlite3_errmsg(db));
        sqlite3_free(error);
        return false;
    }
    return true;
}

static bool sqlite_open_db(sqlite3 **db) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "atm.db")) return false;
    if (sqlite3_open(path, db) != SQLITE_OK) {
        fprintf(stderr, "Unable to open SQLite database: %s\n", *db != NULL ? sqlite3_errmsg(*db) : "unknown error");
        if (*db != NULL) sqlite3_close(*db);
        *db = NULL;
        return false;
    }
    sqlite3_busy_timeout(*db, 2000);
    return sqlite_exec(*db, "PRAGMA foreign_keys = ON;");
}

static bool sqlite_create_schema(sqlite3 *db) {
    const char *schema =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE,"
        "password TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS accounts ("
        "id INTEGER PRIMARY KEY,"
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "account_number INTEGER NOT NULL UNIQUE,"
        "created TEXT NOT NULL,"
        "country TEXT NOT NULL,"
        "phone TEXT NOT NULL,"
        "balance REAL NOT NULL CHECK(balance >= 0),"
        "type TEXT NOT NULL CHECK(type IN ('current','savings','fixed01','fixed02','fixed03')));"
        "CREATE INDEX IF NOT EXISTS idx_accounts_user_id ON accounts(user_id);";
    return sqlite_exec(db, schema);
}

static bool sqlite_load_users(sqlite3 *db, User *users, size_t capacity, size_t *count) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, password FROM users ORDER BY id", -1, &stmt, NULL) != SQLITE_OK) return false;
    *count = 0U;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (*count >= capacity) { sqlite3_finalize(stmt); return false; }
        User *user = &users[*count];
        user->id = sqlite3_column_int(stmt, 0);
        snprintf(user->name, sizeof(user->name), "%s", (const char *)sqlite3_column_text(stmt, 1));
        snprintf(user->password, sizeof(user->password), "%s", (const char *)sqlite3_column_text(stmt, 2));
        (*count)++;
    }
    bool ok = rc == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static bool sqlite_save_users(sqlite3 *db, const User *users, size_t count) {
    const char *sql = "INSERT INTO users(id,name,password) VALUES(?,?,?) "
                      "ON CONFLICT(id) DO UPDATE SET name=excluded.name,password=excluded.password";
    sqlite3_stmt *stmt = NULL;
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { sqlite_exec(db, "ROLLBACK"); return false; }
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        sqlite3_bind_int(stmt, 1, users[i].id);
        sqlite3_bind_text(stmt, 2, users[i].name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, users[i].password, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    if (!ok) { sqlite_exec(db, "ROLLBACK"); return false; }
    return sqlite_exec(db, "COMMIT");
}

static bool sqlite_load_accounts(sqlite3 *db, Account *accounts, size_t capacity, size_t *count) {
    const char *sql = "SELECT a.id,a.user_id,u.name,a.account_number,a.created,a.country,a.phone,a.balance,a.type "
                      "FROM accounts a JOIN users u ON u.id=a.user_id ORDER BY a.id";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    *count = 0U;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (*count >= capacity) { sqlite3_finalize(stmt); return false; }
        Account *a = &accounts[*count];
        a->id = sqlite3_column_int(stmt, 0);
        a->user_id = sqlite3_column_int(stmt, 1);
        snprintf(a->owner, sizeof(a->owner), "%s", (const char *)sqlite3_column_text(stmt, 2));
        a->number = (long long)sqlite3_column_int64(stmt, 3);
        const char *created = (const char *)sqlite3_column_text(stmt, 4);
        const char *type = (const char *)sqlite3_column_text(stmt, 8);
        if (created == NULL || type == NULL || !parse_date(created, &a->created)) { sqlite3_finalize(stmt); return false; }
        snprintf(a->country, sizeof(a->country), "%s", (const char *)sqlite3_column_text(stmt, 5));
        snprintf(a->phone, sizeof(a->phone), "%s", (const char *)sqlite3_column_text(stmt, 6));
        a->balance = sqlite3_column_double(stmt, 7);
        a->type = account_type_from_string(type);
        if (a->type == ACCOUNT_INVALID) { sqlite3_finalize(stmt); return false; }
        (*count)++;
    }
    bool ok = rc == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static bool sqlite_save_accounts(sqlite3 *db, const Account *accounts, size_t count) {
    const char *sql = "INSERT INTO accounts(id,user_id,account_number,created,country,phone,balance,type) VALUES(?,?,?,?,?,?,?,?)";
    sqlite3_stmt *stmt = NULL;
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return false;
    if (!sqlite_exec(db, "DELETE FROM accounts")) { sqlite_exec(db, "ROLLBACK"); return false; }
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { sqlite_exec(db, "ROLLBACK"); return false; }
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        const Account *a = &accounts[i];
        char date[32];
        snprintf(date, sizeof(date), "%02d/%02d/%04d", a->created.day, a->created.month, a->created.year);
        sqlite3_bind_int(stmt, 1, a->id);
        sqlite3_bind_int(stmt, 2, a->user_id);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)a->number);
        sqlite3_bind_text(stmt, 4, date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, a->country, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, a->phone, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 7, a->balance);
        sqlite3_bind_text(stmt, 8, account_type_name(a->type), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    if (!ok) { sqlite_exec(db, "ROLLBACK"); return false; }
    return sqlite_exec(db, "COMMIT");
}

static bool sqlite_seed_if_empty(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users", -1, &stmt, NULL) != SQLITE_OK) return false;
    int rc = sqlite3_step(stmt);
    int count = rc == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
    sqlite3_finalize(stmt);
    if (count < 0) return false;
    if (count > 0) return true;

    User users[ATM_MAX_USERS];
    size_t user_count = 0U;
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t account_count = 0U;
    if (!text_load_users(users, ATM_MAX_USERS, &user_count)) return false;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &account_count)) return false;
    if (!sqlite_save_users(db, users, user_count)) return false;
    return sqlite_save_accounts(db, accounts, account_count);
}
#endif

bool ensure_data_files(void) {
    if (ATM_MKDIR(data_dir()) != 0 && errno != EEXIST) {
        perror("Unable to create data directory");
        return false;
    }
    char users_path[ATM_PATH_LEN], records_path[ATM_PATH_LEN];
    if (!build_path(users_path, "users.txt") || !build_path(records_path, "records.txt")) return false;
    if (!touch_if_missing(users_path) || !touch_if_missing(records_path)) {
        perror("Unable to initialize seed files");
        return false;
    }
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        bool ok = sqlite_open_db(&db) && sqlite_create_schema(db) && sqlite_seed_if_empty(db);
        if (db != NULL) sqlite3_close(db);
        return ok;
    }
#endif
    return true;
}

bool load_users(User *users, size_t capacity, size_t *count) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_load_users(db, users, capacity, count);
        sqlite3_close(db);
        return ok;
    }
#endif
    return text_load_users(users, capacity, count);
}

bool save_users(const User *users, size_t count) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_save_users(db, users, count);
        sqlite3_close(db);
        return ok;
    }
#endif
    return text_save_users(users, count);
}

bool load_accounts(Account *accounts, size_t capacity, size_t *count) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_load_accounts(db, accounts, capacity, count);
        sqlite3_close(db);
        return ok;
    }
#endif
    return text_load_accounts(accounts, capacity, count);
}

bool save_accounts(const Account *accounts, size_t count) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_save_accounts(db, accounts, count);
        sqlite3_close(db);
        return ok;
    }
#endif
    return text_save_accounts(accounts, count);
}
