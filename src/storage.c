#include "header.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (file == NULL) {
        return false;
    }
    return fclose(file) == 0;
}

bool ensure_data_files(void) {
    if (ATM_MKDIR(data_dir()) != 0 && errno != EEXIST) {
        perror("Unable to create data directory");
        return false;
    }

    char users_path[ATM_PATH_LEN];
    char records_path[ATM_PATH_LEN];
    if (!build_path(users_path, "users.txt") || !build_path(records_path, "records.txt")) {
        fputs("Data path is too long.\n", stderr);
        return false;
    }

    if (!touch_if_missing(users_path) || !touch_if_missing(records_path)) {
        perror("Unable to initialize data files");
        return false;
    }
    return true;
}

bool load_users(User *users, size_t capacity, size_t *count) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "users.txt")) {
        return false;
    }

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

bool save_users(const User *users, size_t count) {
    char path[ATM_PATH_LEN];
    char temp[ATM_PATH_LEN];
    if (!build_path(path, "users.txt") || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) {
        return false;
    }

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

    if (fclose(file) != 0) {
        ok = false;
    }
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

bool load_accounts(Account *accounts, size_t capacity, size_t *count) {
    char path[ATM_PATH_LEN];
    if (!build_path(path, "records.txt")) {
        return false;
    }

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
        char date_text[32];
        char type_text[ATM_TYPE_LEN];
        int parsed = sscanf(line, "%d %d %63s %lld %31s %63s %31s %lf %15s",
                            &account.id, &account.user_id, account.owner, &account.number,
                            date_text, account.country, account.phone, &account.balance, type_text);
        if (parsed == 9 && parse_date(date_text, &account.created)) {
            account.type = account_type_from_string(type_text);
            if (account.type != ACCOUNT_INVALID) {
                accounts[(*count)++] = account;
            }
        }
    }

    bool ok = !ferror(file);
    fclose(file);
    return ok;
}

bool save_accounts(const Account *accounts, size_t count) {
    char path[ATM_PATH_LEN];
    char temp[ATM_PATH_LEN];
    if (!build_path(path, "records.txt") || snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) {
        return false;
    }

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

    if (fclose(file) != 0) {
        ok = false;
    }
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
