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

/* utils.c */
bool read_line(const char *prompt, char *buffer, size_t size);
bool prompt_int(const char *prompt, int *value);
bool prompt_long_long(const char *prompt, long long *value);
bool prompt_double(const char *prompt, double *value);
void trim_whitespace(char *text);

/* password.c */
void hash_password(const char *password, char output[ATM_PASSWORD_LEN]);
bool password_matches(const char *password, const char *stored);

/* storage.c */
bool ensure_data_files(void);
bool load_users(User *users, size_t capacity, size_t *count);
bool save_users(const User *users, size_t count);
bool load_accounts(Account *accounts, size_t capacity, size_t *count);
bool save_accounts(const Account *accounts, size_t count);

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

/* system.c */
void account_menu(User *user, NotificationSession *notifications);

/* notify.c */
bool notification_start(const User *user, NotificationSession *session);
void notification_stop(NotificationSession *session);
void notification_send(const char *username, const char *message);

#endif
