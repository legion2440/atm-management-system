#include "header.h"

#include <errno.h>
#include <math.h>
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

typedef struct {
    char name[ATM_NAME_LEN];
    int failed_attempts;
    long long locked_until;
} LoginSecurity;

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
        if (sscanf(line, "%d %63s %127s", &user.id, user.name, user.password) == 3) {
            users[(*count)++] = user;
        }
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
        if (fprintf(file, "%d %s %s\n", users[i].id, users[i].name, users[i].password) < 0) {
            ok = false;
            break;
        }
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temp);
        return false;
    }
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
                    a->country, a->phone, a->balance, account_type_name(a->type)) < 0) {
            ok = false;
            break;
        }
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temp);
        return false;
    }
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

static bool text_load_security(LoginSecurity *entries, size_t capacity, size_t *count) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "login_security.txt")) return false;
    FILE *file = fopen(path, "a+");
    if (file == NULL) return false;
    rewind(file);
    *count = 0U;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (*count >= capacity) {
            fclose(file);
            return false;
        }
        LoginSecurity entry;
        if (sscanf(line, "%63s %d %lld", entry.name, &entry.failed_attempts, &entry.locked_until) == 3) {
            entries[(*count)++] = entry;
        }
    }
    bool ok = !ferror(file);
    fclose(file);
    return ok;
}

static bool text_save_security(const LoginSecurity *entries, size_t count) {
    char path[ATM_PATH_LEN], temp[ATM_PATH_LEN];
    if (!build_path(path, "login_security.txt") || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) return false;
    FILE *file = fopen(temp, "w");
    if (file == NULL) return false;
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        if (fprintf(file, "%s %d %lld\n", entries[i].name, entries[i].failed_attempts,
                    entries[i].locked_until) < 0) {
            ok = false;
            break;
        }
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temp);
        return false;
    }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(temp, path) != 0) {
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
    sqlite3_busy_timeout(*db, 5000);
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
        "CREATE INDEX IF NOT EXISTS idx_accounts_user_id ON accounts(user_id);"
        "CREATE TABLE IF NOT EXISTS login_security ("
        "name TEXT PRIMARY KEY,"
        "failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK(failed_attempts >= 0),"
        "locked_until INTEGER NOT NULL DEFAULT 0 CHECK(locked_until >= 0));";
    return sqlite_exec(db, schema);
}

static bool sqlite_load_users(sqlite3 *db, User *users, size_t capacity, size_t *count) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, password FROM users ORDER BY id", -1, &stmt, NULL) != SQLITE_OK) return false;
    *count = 0U;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (*count >= capacity) {
            sqlite3_finalize(stmt);
            return false;
        }
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
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    bool ok = true;
    for (size_t i = 0U; i < count; ++i) {
        sqlite3_bind_int(stmt, 1, users[i].id);
        sqlite3_bind_text(stmt, 2, users[i].name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, users[i].password, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            ok = false;
            break;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    if (!ok) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
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
        if (*count >= capacity) {
            sqlite3_finalize(stmt);
            return false;
        }
        Account *a = &accounts[*count];
        a->id = sqlite3_column_int(stmt, 0);
        a->user_id = sqlite3_column_int(stmt, 1);
        snprintf(a->owner, sizeof(a->owner), "%s", (const char *)sqlite3_column_text(stmt, 2));
        a->number = (long long)sqlite3_column_int64(stmt, 3);
        const char *created = (const char *)sqlite3_column_text(stmt, 4);
        const char *type = (const char *)sqlite3_column_text(stmt, 8);
        if (created == NULL || type == NULL || !parse_date(created, &a->created)) {
            sqlite3_finalize(stmt);
            return false;
        }
        snprintf(a->country, sizeof(a->country), "%s", (const char *)sqlite3_column_text(stmt, 5));
        snprintf(a->phone, sizeof(a->phone), "%s", (const char *)sqlite3_column_text(stmt, 6));
        a->balance = sqlite3_column_double(stmt, 7);
        a->type = account_type_from_string(type);
        if (a->type == ACCOUNT_INVALID) {
            sqlite3_finalize(stmt);
            return false;
        }
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
    if (!sqlite_exec(db, "DELETE FROM accounts")) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
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
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            ok = false;
            break;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    if (!ok) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
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

static StorageResult sqlite_create_user_record(sqlite3 *db, const char *name, const char *password_hash, User *created) {
    sqlite3_stmt *stmt = NULL;
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return STORAGE_RESULT_ERROR;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    int rc = sqlite3_step(stmt);
    int count = rc == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
    sqlite3_finalize(stmt);
    if (count < 0) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    if (count >= ATM_MAX_USERS) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_FULL;
    }

    if (sqlite3_prepare_v2(db, "INSERT INTO users(name,password) VALUES(?,?)", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_CONFLICT;
    }
    if (rc != SQLITE_DONE) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }

    int id = (int)sqlite3_last_insert_rowid(db);
    if (!sqlite_exec(db, "COMMIT")) return STORAGE_RESULT_ERROR;
    if (created != NULL) {
        created->id = id;
        snprintf(created->name, sizeof(created->name), "%s", name);
        snprintf(created->password, sizeof(created->password), "%s", password_hash);
    }
    return STORAGE_RESULT_OK;
}

static StorageResult sqlite_update_password_record(sqlite3 *db, int user_id, const char *name, const char *password_hash) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE users SET password=? WHERE id=? AND name=?", -1, &stmt, NULL) != SQLITE_OK) {
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_text(stmt, 1, password_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STORAGE_RESULT_ERROR;
    return changed == 1 ? STORAGE_RESULT_OK : STORAGE_RESULT_NOT_FOUND;
}

static bool sqlite_login_locked_record(sqlite3 *db, const char *name, long long now, long long *locked_until) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT locked_until FROM login_security WHERE name=?", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    long long until = rc == SQLITE_ROW ? (long long)sqlite3_column_int64(stmt, 0) : 0LL;
    sqlite3_finalize(stmt);
    if (locked_until != NULL) *locked_until = until;
    return until > now;
}

static bool sqlite_login_failure_record(sqlite3 *db, const char *name, long long now, int max_attempts,
                                        long long lock_seconds, long long *locked_until) {
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return false;
    sqlite3_stmt *stmt = NULL;
    int attempts = 0;
    long long until = 0LL;

    if (sqlite3_prepare_v2(db, "SELECT failed_attempts,locked_until FROM login_security WHERE name=?", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        attempts = sqlite3_column_int(stmt, 0);
        until = (long long)sqlite3_column_int64(stmt, 1);
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    sqlite3_finalize(stmt);

    if (until <= now) {
        attempts = 0;
        until = 0LL;
    }
    if (until == 0LL) {
        attempts++;
        if (attempts >= max_attempts) {
            attempts = max_attempts;
            until = now + lock_seconds;
        }
    }

    const char *sql = "INSERT INTO login_security(name,failed_attempts,locked_until) VALUES(?,?,?) "
                      "ON CONFLICT(name) DO UPDATE SET failed_attempts=excluded.failed_attempts,locked_until=excluded.locked_until";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, attempts);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)until);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE || !sqlite_exec(db, "COMMIT")) {
        sqlite_exec(db, "ROLLBACK");
        return false;
    }
    if (locked_until != NULL) *locked_until = until;
    return true;
}

static bool sqlite_login_success_record(sqlite3 *db, const char *name) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM login_security WHERE name=?", -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static StorageResult sqlite_account_create_record(sqlite3 *db, const Account *a) {
    sqlite3_stmt *stmt = NULL;
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return STORAGE_RESULT_ERROR;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM accounts", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    int rc = sqlite3_step(stmt);
    int count = rc == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
    sqlite3_finalize(stmt);
    if (count < 0) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    if (count >= ATM_MAX_ACCOUNTS) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_FULL;
    }

    const char *sql = "INSERT INTO accounts(user_id,account_number,created,country,phone,balance,type) VALUES(?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    char date[32];
    snprintf(date, sizeof(date), "%02d/%02d/%04d", a->created.day, a->created.month, a->created.year);
    sqlite3_bind_int(stmt, 1, a->user_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)a->number);
    sqlite3_bind_text(stmt, 3, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, a->country, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, a->phone, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, a->balance);
    sqlite3_bind_text(stmt, 7, account_type_name(a->type), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_CONFLICT;
    }
    if (rc != SQLITE_DONE || !sqlite_exec(db, "COMMIT")) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    return STORAGE_RESULT_OK;
}

static StorageResult sqlite_account_update_contact_record(sqlite3 *db, int user_id, long long number,
                                                          bool update_phone, const char *value) {
    const char *sql = update_phone
        ? "UPDATE accounts SET phone=? WHERE user_id=? AND account_number=?"
        : "UPDATE accounts SET country=? WHERE user_id=? AND account_number=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STORAGE_RESULT_ERROR;
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)number);
    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STORAGE_RESULT_ERROR;
    return changed == 1 ? STORAGE_RESULT_OK : STORAGE_RESULT_NOT_FOUND;
}

static StorageResult sqlite_account_transaction_record(sqlite3 *db, int user_id, long long number,
                                                       bool deposit, double amount, double *new_balance) {
    if (!(amount > 0.0) || !isfinite(amount)) return STORAGE_RESULT_DENIED;
    if (!sqlite_exec(db, "BEGIN IMMEDIATE")) return STORAGE_RESULT_ERROR;

    sqlite3_stmt *stmt = NULL;
    const char *select_sql = "SELECT balance,type FROM accounts WHERE user_id=? AND account_number=?";
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)number);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite_exec(db, "ROLLBACK");
        return rc == SQLITE_DONE ? STORAGE_RESULT_NOT_FOUND : STORAGE_RESULT_ERROR;
    }
    double balance = sqlite3_column_double(stmt, 0);
    const char *type_text = (const char *)sqlite3_column_text(stmt, 1);
    AccountType type = type_text != NULL ? account_type_from_string(type_text) : ACCOUNT_INVALID;
    sqlite3_finalize(stmt);
    if (type != ACCOUNT_CURRENT && type != ACCOUNT_SAVINGS) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_DENIED;
    }
    if (!deposit && amount > balance) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_INSUFFICIENT;
    }

    double updated = deposit ? balance + amount : balance - amount;
    if (!isfinite(updated) || updated < 0.0) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_DENIED;
    }
    if (sqlite3_prepare_v2(db, "UPDATE accounts SET balance=? WHERE user_id=? AND account_number=?", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_double(stmt, 1, updated);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)number);
    rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE || changed != 1 || !sqlite_exec(db, "COMMIT")) {
        sqlite_exec(db, "ROLLBACK");
        return STORAGE_RESULT_ERROR;
    }
    if (new_balance != NULL) *new_balance = updated;
    return STORAGE_RESULT_OK;
}

static StorageResult sqlite_account_delete_record(sqlite3 *db, int user_id, long long number) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM accounts WHERE user_id=? AND account_number=?", -1, &stmt, NULL) != SQLITE_OK) {
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)number);
    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STORAGE_RESULT_ERROR;
    return changed == 1 ? STORAGE_RESULT_OK : STORAGE_RESULT_NOT_FOUND;
}

static StorageResult sqlite_account_transfer_record(sqlite3 *db, int user_id, long long number, int target_user_id) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE accounts SET user_id=? WHERE user_id=? AND account_number=?", -1, &stmt, NULL) != SQLITE_OK) {
        return STORAGE_RESULT_ERROR;
    }
    sqlite3_bind_int(stmt, 1, target_user_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)number);
    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) return STORAGE_RESULT_CONFLICT;
    if (rc != SQLITE_DONE) return STORAGE_RESULT_ERROR;
    return changed == 1 ? STORAGE_RESULT_OK : STORAGE_RESULT_NOT_FOUND;
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
        bool ok = sqlite_create_schema(db) && sqlite_load_users(db, users, capacity, count);
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
        bool ok = sqlite_create_schema(db) && sqlite_save_users(db, users, count);
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
        bool ok = sqlite_create_schema(db) && sqlite_load_accounts(db, accounts, capacity, count);
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
        bool ok = sqlite_create_schema(db) && sqlite_save_accounts(db, accounts, count);
        sqlite3_close(db);
        return ok;
    }
#endif
    return text_save_accounts(accounts, count);
}

StorageResult storage_create_user(const char *name, const char *password_hash, User *created) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_create_user_record(db, name, password_hash, created)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    User users[ATM_MAX_USERS];
    size_t count = 0U;
    if (!text_load_users(users, ATM_MAX_USERS, &count)) return STORAGE_RESULT_ERROR;
    if (count >= ATM_MAX_USERS) return STORAGE_RESULT_FULL;
    int max_id = -1;
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(users[i].name, name) == 0) return STORAGE_RESULT_CONFLICT;
        if (users[i].id > max_id) max_id = users[i].id;
    }
    User user;
    user.id = max_id + 1;
    snprintf(user.name, sizeof(user.name), "%s", name);
    snprintf(user.password, sizeof(user.password), "%s", password_hash);
    users[count++] = user;
    if (!text_save_users(users, count)) return STORAGE_RESULT_ERROR;
    if (created != NULL) *created = user;
    return STORAGE_RESULT_OK;
}

StorageResult storage_update_password(int user_id, const char *name, const char *password_hash) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_update_password_record(db, user_id, name, password_hash)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    User users[ATM_MAX_USERS];
    size_t count = 0U;
    if (!text_load_users(users, ATM_MAX_USERS, &count)) return STORAGE_RESULT_ERROR;
    for (size_t i = 0U; i < count; ++i) {
        if (users[i].id == user_id && strcmp(users[i].name, name) == 0) {
            snprintf(users[i].password, sizeof(users[i].password), "%s", password_hash);
            return text_save_users(users, count) ? STORAGE_RESULT_OK : STORAGE_RESULT_ERROR;
        }
    }
    return STORAGE_RESULT_NOT_FOUND;
}

bool storage_login_locked(const char *name, long long now, long long *locked_until) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool locked = sqlite_create_schema(db) && sqlite_login_locked_record(db, name, now, locked_until);
        sqlite3_close(db);
        return locked;
    }
#endif
    LoginSecurity entries[ATM_MAX_USERS];
    size_t count = 0U;
    if (!text_load_security(entries, ATM_MAX_USERS, &count)) return false;
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(entries[i].name, name) == 0) {
            if (locked_until != NULL) *locked_until = entries[i].locked_until;
            return entries[i].locked_until > now;
        }
    }
    if (locked_until != NULL) *locked_until = 0LL;
    return false;
}

bool storage_login_failure(const char *name, long long now, int max_attempts, long long lock_seconds,
                           long long *locked_until) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_create_schema(db) &&
                  sqlite_login_failure_record(db, name, now, max_attempts, lock_seconds, locked_until);
        sqlite3_close(db);
        return ok;
    }
#endif
    LoginSecurity entries[ATM_MAX_USERS];
    size_t count = 0U;
    if (!text_load_security(entries, ATM_MAX_USERS, &count)) return false;
    size_t index = count;
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(entries[i].name, name) == 0) {
            index = i;
            break;
        }
    }
    if (index == count) {
        if (count >= ATM_MAX_USERS) return false;
        snprintf(entries[index].name, sizeof(entries[index].name), "%s", name);
        entries[index].failed_attempts = 0;
        entries[index].locked_until = 0LL;
        count++;
    }
    if (entries[index].locked_until <= now) {
        entries[index].failed_attempts = 0;
        entries[index].locked_until = 0LL;
    }
    if (entries[index].locked_until == 0LL) {
        entries[index].failed_attempts++;
        if (entries[index].failed_attempts >= max_attempts) {
            entries[index].failed_attempts = max_attempts;
            entries[index].locked_until = now + lock_seconds;
        }
    }
    if (locked_until != NULL) *locked_until = entries[index].locked_until;
    return text_save_security(entries, count);
}

bool storage_login_success(const char *name) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return false;
        bool ok = sqlite_create_schema(db) && sqlite_login_success_record(db, name);
        sqlite3_close(db);
        return ok;
    }
#endif
    LoginSecurity entries[ATM_MAX_USERS];
    size_t count = 0U;
    if (!text_load_security(entries, ATM_MAX_USERS, &count)) return false;
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(entries[i].name, name) == 0) {
            for (size_t j = i + 1U; j < count; ++j) entries[j - 1U] = entries[j];
            count--;
            break;
        }
    }
    return text_save_security(entries, count);
}

StorageResult storage_account_create(const Account *account) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_account_create_record(db, account)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return STORAGE_RESULT_ERROR;
    if (count >= ATM_MAX_ACCOUNTS) return STORAGE_RESULT_FULL;
    int max_id = -1;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].number == account->number) return STORAGE_RESULT_CONFLICT;
        if (accounts[i].id > max_id) max_id = accounts[i].id;
    }
    Account copy = *account;
    copy.id = max_id + 1;
    accounts[count++] = copy;
    return text_save_accounts(accounts, count) ? STORAGE_RESULT_OK : STORAGE_RESULT_ERROR;
}

StorageResult storage_account_update_contact(int user_id, long long number, bool update_phone, const char *value) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_account_update_contact_record(db, user_id, number, update_phone, value)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return STORAGE_RESULT_ERROR;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id == user_id && accounts[i].number == number) {
            if (update_phone) snprintf(accounts[i].phone, sizeof(accounts[i].phone), "%s", value);
            else snprintf(accounts[i].country, sizeof(accounts[i].country), "%s", value);
            return text_save_accounts(accounts, count) ? STORAGE_RESULT_OK : STORAGE_RESULT_ERROR;
        }
    }
    return STORAGE_RESULT_NOT_FOUND;
}

StorageResult storage_account_transaction(int user_id, long long number, bool deposit, double amount,
                                          double *new_balance) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_account_transaction_record(db, user_id, number, deposit, amount, new_balance)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return STORAGE_RESULT_ERROR;
    for (size_t i = 0U; i < count; ++i) {
        Account *a = &accounts[i];
        if (a->user_id != user_id || a->number != number) continue;
        if (a->type != ACCOUNT_CURRENT && a->type != ACCOUNT_SAVINGS) return STORAGE_RESULT_DENIED;
        if (!(amount > 0.0) || !isfinite(amount)) return STORAGE_RESULT_DENIED;
        if (!deposit && amount > a->balance) return STORAGE_RESULT_INSUFFICIENT;
        double updated = deposit ? a->balance + amount : a->balance - amount;
        if (!isfinite(updated) || updated < 0.0) return STORAGE_RESULT_DENIED;
        a->balance = updated;
        if (!text_save_accounts(accounts, count)) return STORAGE_RESULT_ERROR;
        if (new_balance != NULL) *new_balance = updated;
        return STORAGE_RESULT_OK;
    }
    return STORAGE_RESULT_NOT_FOUND;
}

StorageResult storage_account_delete(int user_id, long long number) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_account_delete_record(db, user_id, number)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return STORAGE_RESULT_ERROR;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id == user_id && accounts[i].number == number) {
            for (size_t j = i + 1U; j < count; ++j) accounts[j - 1U] = accounts[j];
            count--;
            return text_save_accounts(accounts, count) ? STORAGE_RESULT_OK : STORAGE_RESULT_ERROR;
        }
    }
    return STORAGE_RESULT_NOT_FOUND;
}

StorageResult storage_account_transfer(int user_id, long long number, int target_user_id) {
#ifndef ATM_NO_SQLITE
    if (storage_uses_sqlite()) {
        sqlite3 *db = NULL;
        if (!sqlite_open_db(&db)) return STORAGE_RESULT_ERROR;
        StorageResult result = sqlite_create_schema(db)
            ? sqlite_account_transfer_record(db, user_id, number, target_user_id)
            : STORAGE_RESULT_ERROR;
        sqlite3_close(db);
        return result;
    }
#endif
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!text_load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return STORAGE_RESULT_ERROR;
    User target;
    if (!find_user_by_name("", NULL)) {
        (void)target;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id == user_id && accounts[i].number == number) {
            accounts[i].user_id = target_user_id;
            User users[ATM_MAX_USERS];
            size_t user_count = 0U;
            if (!text_load_users(users, ATM_MAX_USERS, &user_count)) return STORAGE_RESULT_ERROR;
            bool found = false;
            for (size_t j = 0U; j < user_count; ++j) {
                if (users[j].id == target_user_id) {
                    snprintf(accounts[i].owner, sizeof(accounts[i].owner), "%s", users[j].name);
                    found = true;
                    break;
                }
            }
            if (!found) return STORAGE_RESULT_CONFLICT;
            return text_save_accounts(accounts, count) ? STORAGE_RESULT_OK : STORAGE_RESULT_ERROR;
        }
    }
    return STORAGE_RESULT_NOT_FOUND;
}
