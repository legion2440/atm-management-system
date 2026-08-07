#include "header.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void) {
    puts("concurrency tests: SKIP on native Windows");
    return 0;
}
#else
#include <sys/wait.h>
#include <unistd.h>

static int write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    if (file == NULL) return 0;
    int ok = fputs(content, file) >= 0 && fclose(file) == 0;
    return ok;
}

static const Account *find_account(const Account *accounts, size_t count, long long number) {
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].number == number) return &accounts[i];
    }
    return NULL;
}

int main(void) {
    char temp[] = "/tmp/atm-concurrency-XXXXXX";
    char *dir = mkdtemp(temp);
    if (dir == NULL || setenv("ATM_DATA_DIR", dir, 1) != 0) return 1;

    char users[ATM_PATH_LEN];
    char records[ATM_PATH_LEN];
    char db[ATM_PATH_LEN];
    snprintf(users, sizeof(users), "%s/users.txt", dir);
    snprintf(records, sizeof(records), "%s/records.txt", dir);
    snprintf(db, sizeof(db), "%s/atm.db", dir);
    if (!write_file(users, "0 Alice legacy-password\n") || !write_file(records, "")) return 1;
    if (!ensure_data_files()) return 1;

    Account first = {0};
    first.user_id = 0;
    snprintf(first.owner, sizeof(first.owner), "Alice");
    first.number = 9000;
    first.created = (Date){1, 1, 2024};
    snprintf(first.country, sizeof(first.country), "KZ");
    snprintf(first.phone, sizeof(first.phone), "1");
    first.balance = 0.0;
    first.type = ACCOUNT_CURRENT;
    if (storage_account_create(&first) != STORAGE_RESULT_OK) return 1;

    const int workers = 4;
    const int deposits = 50;
    pid_t children[workers];
    for (int i = 0; i < workers; ++i) {
        children[i] = fork();
        if (children[i] < 0) return 1;
        if (children[i] == 0) {
            for (int j = 0; j < deposits; ++j) {
                double balance = 0.0;
                if (storage_account_transaction(0, 9000, true, 1.0, &balance) != STORAGE_RESULT_OK) {
                    _exit(2);
                }
            }
            _exit(0);
        }
    }
    for (int i = 0; i < workers; ++i) {
        int status = 0;
        if (waitpid(children[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
    }

    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count = 0U;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return 1;
    const Account *saved = find_account(accounts, count, 9000);
    if (saved == NULL || fabs(saved->balance - 200.0) > 0.001) return 1;
    puts("[PASS] Concurrent deposits do not lose updates");

    Account second = first;
    second.number = 9001;
    second.balance = 100.0;
    if (storage_account_create(&second) != STORAGE_RESULT_OK) return 1;

    pid_t withdrawals[2];
    for (int i = 0; i < 2; ++i) {
        withdrawals[i] = fork();
        if (withdrawals[i] < 0) return 1;
        if (withdrawals[i] == 0) {
            double balance = 0.0;
            StorageResult result = storage_account_transaction(0, 9001, false, 75.0, &balance);
            if (result == STORAGE_RESULT_OK) _exit(0);
            if (result == STORAGE_RESULT_INSUFFICIENT) _exit(2);
            _exit(3);
        }
    }

    int successful = 0;
    int denied = 0;
    for (int i = 0; i < 2; ++i) {
        int status = 0;
        if (waitpid(withdrawals[i], &status, 0) < 0 || !WIFEXITED(status)) return 1;
        if (WEXITSTATUS(status) == 0) successful++;
        else if (WEXITSTATUS(status) == 2) denied++;
        else return 1;
    }
    if (successful != 1 || denied != 1) return 1;

    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) return 1;
    saved = find_account(accounts, count, 9001);
    if (saved == NULL || fabs(saved->balance - 25.0) > 0.001) return 1;
    puts("[PASS] Concurrent withdrawals cannot overdraw an account");

    remove(db);
    remove(users);
    remove(records);
    (void)rmdir(dir);
    puts("2/2 concurrency cases passed");
    return 0;
}
#endif
