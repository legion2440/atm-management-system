#ifndef ATM_HEADER_H
#define ATM_HEADER_H

#include <stdbool.h>
#include <stddef.h>

#define ATM_NAME_LEN 64
#define ATM_PASSWORD_LEN 128
#define ATM_COUNTRY_LEN 64
#define ATM_PHONE_LEN 32
#define ATM_TYPE_LEN 16
#define ATM_PATH_LEN 512
#define ATM_MAX_USERS 2048
#define ATM_MAX_ACCOUNTS 8192

typedef struct {
    int day;
    int month;
    int year;
} Date;

typedef enum {
    ACCOUNT_INVALID = 0,
    ACCOUNT_CURRENT,
    ACCOUNT_SAVINGS,
    ACCOUNT_FIXED01,
    ACCOUNT_FIXED02,
    ACCOUNT_FIXED03
} AccountType;

typedef struct {
    int id;
    char name[ATM_NAME_LEN];
    char password[ATM_PASSWORD_LEN];
} User;

typedef struct {
    int id;
    int user_id;
    char owner[ATM_NAME_LEN];
    long long number;
    Date created;
    char country[ATM_COUNTRY_LEN];
    char phone[ATM_PHONE_LEN];
    double balance;
    AccountType type;
} Account;

typedef struct {
    long child_pid;
    char fifo_path[ATM_PATH_LEN];
    bool active;
} NotificationSession;

typedef enum {
    STORAGE_RESULT_OK = 0,
    STORAGE_RESULT_NOT_FOUND,
    STORAGE_RESULT_CONFLICT,
    STORAGE_RESULT_DENIED,
    STORAGE_RESULT_INSUFFICIENT,
    STORAGE_RESULT_FULL,
    STORAGE_RESULT_ERROR
} StorageResult;

/* utils.c */
bool read_line(const char *prompt, char *buffer, size_t size);
bool prompt_int(const char *prompt, int *value);
bool prompt_long_long(const char *prompt, long long *value);
bool prompt_double(const char *prompt, double *value);
void trim_whitespace(char *text);
bool contains_whitespace(const char *text);

/* ui.c */
void ui_banner(void);
void ui_session_header(const User *user);
void ui_section(const char *title);

/* password.c */
void hash_password(const char *password, char output[ATM_PASSWORD_LEN]);
bool password_matches(const char *password, const char *stored);
bool password_needs_upgrade(const char *stored);

/* storage.c */
bool ensure_data_files(void);
bool load_users(User *users, size_t capacity, size_t *count);
bool save_users(const User *users, size_t count);
bool load_accounts(Account *accounts, size_t capacity, size_t *count);
bool save_accounts(const Account *accounts, size_t count);
const char *storage_backend_name(void);
bool storage_uses_sqlite(void);
StorageResult storage_create_user(const char *name, const char *password_hash, User *created);
StorageResult storage_update_password(int user_id, const char *name, const char *password_hash);
bool storage_login_locked(const char *name, long long now, long long *locked_until);
bool storage_login_failure(const char *name, long long now, int max_attempts, long long lock_seconds,
                           long long *locked_until);
bool storage_login_success(const char *name);
StorageResult storage_account_create(const Account *account);
StorageResult storage_account_update_contact(int user_id, long long number, bool update_phone, const char *value);
StorageResult storage_account_transaction(int user_id, long long number, bool deposit, double amount,
                                          double *new_balance);
StorageResult storage_account_delete(int user_id, long long number);
StorageResult storage_account_transfer(int user_id, long long number, int target_user_id);

/* account.c */
AccountType account_type_from_string(const char *text);
const char *account_type_name(AccountType type);
bool parse_date(const char *text, Date *date);
bool date_is_valid(Date date);
double account_interest(const Account *account);
Date account_interest_date(const Account *account);

/* auth.c */
bool register_user_interactive(void);
bool login_user_interactive(User *user);
bool find_user_by_name(const char *name, User *user);
bool change_password_interactive(User *user);

/* system.c */
void account_menu(User *user, NotificationSession *notifications);

/* notify.c */
bool notification_start(const User *user, NotificationSession *session);
void notification_stop(NotificationSession *session);
void notification_send(const char *username, const char *message);

#endif
