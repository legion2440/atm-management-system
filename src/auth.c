#include "header.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOGIN_MAX_ATTEMPTS 5
#define LOGIN_LOCK_SECONDS 30LL

static long long login_lock_seconds(void) {
    const char *configured = getenv("ATM_LOGIN_LOCK_SECONDS");
    if (configured == NULL || *configured == '\0') return LOGIN_LOCK_SECONDS;

    char *end = NULL;
    errno = 0;
    long long seconds = strtoll(configured, &end, 10);
    if (errno != 0 || end == configured || *end != '\0' || seconds < 1LL || seconds > 86400LL) {
        return LOGIN_LOCK_SECONDS;
    }
    return seconds;
}

static void dummy_password_work(const char *password) {
    char ignored[ATM_PASSWORD_LEN];
    hash_password(password, ignored);
}

bool find_user_by_name(const char *name, User *user) {
    User users[ATM_MAX_USERS];
    size_t count;
    if (!load_users(users, ATM_MAX_USERS, &count)) return false;

    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            if (user != NULL) *user = users[i];
            return true;
        }
    }
    return false;
}

bool register_user_interactive(void) {
    char name[ATM_NAME_LEN];
    char password[ATM_PASSWORD_LEN];
    if (!read_line("Username: ", name, sizeof(name)) || !read_line("Password: ", password, sizeof(password))) {
        return false;
    }

    if (name[0] == '\0' || password[0] == '\0' || contains_whitespace(name)) {
        puts("Username and password must be non-empty; usernames cannot contain whitespace.");
        return false;
    }

    char password_hash[ATM_PASSWORD_LEN];
    hash_password(password, password_hash);
    StorageResult result = storage_create_user(name, password_hash, NULL);
    if (result == STORAGE_RESULT_CONFLICT) {
        printf("User '%s' already exists.\n", name);
        return false;
    }
    if (result == STORAGE_RESULT_FULL) {
        puts("User storage is full.");
        return false;
    }
    if (result != STORAGE_RESULT_OK) {
        puts("Registration failed while saving data.");
        return false;
    }

    printf("User '%s' registered successfully.\n", name);
    return true;
}

bool login_user_interactive(User *user) {
    char name[ATM_NAME_LEN];
    char password[ATM_PASSWORD_LEN];
    if (!read_line("Username: ", name, sizeof(name)) || !read_line("Password: ", password, sizeof(password))) {
        return false;
    }

    long long now = (long long)time(NULL);
    long long locked_until = 0LL;
    if (storage_login_locked(name, now, &locked_until)) {
        puts("Too many failed login attempts. Try again later.");
        return false;
    }

    User candidate;
    bool found = find_user_by_name(name, &candidate);
    bool matches = false;
    if (found) {
        matches = password_matches(password, candidate.password);
    } else {
        dummy_password_work(password);
    }

    if (found && matches) {
        if (password_needs_upgrade(candidate.password)) {
            char upgraded[ATM_PASSWORD_LEN];
            hash_password(password, upgraded);
            if (storage_update_password(candidate.id, candidate.name, upgraded) != STORAGE_RESULT_OK) {
                puts("Login failed while upgrading stored credentials.");
                return false;
            }
            snprintf(candidate.password, sizeof(candidate.password), "%s", upgraded);
        }
        if (!storage_login_success(name)) {
            puts("Login failed while updating security state.");
            return false;
        }
        *user = candidate;
        printf("Welcome, %s.\n", user->name);
        return true;
    }

    locked_until = 0LL;
    if (!storage_login_failure(name, now, LOGIN_MAX_ATTEMPTS, login_lock_seconds(), &locked_until)) {
        puts("Login failed while updating security state.");
        return false;
    }
    if (locked_until > now) puts("Too many failed login attempts. Try again later.");
    else puts("Invalid username or password.");
    return false;
}

bool change_password_interactive(User *user) {
    User current_user;
    if (!find_user_by_name(user->name, &current_user) || current_user.id != user->id) {
        puts("Current user no longer exists.");
        return false;
    }

    char current[ATM_PASSWORD_LEN];
    char replacement[ATM_PASSWORD_LEN];
    char confirmation[ATM_PASSWORD_LEN];
    if (!read_line("Current password: ", current, sizeof(current))) return false;
    if (!password_matches(current, current_user.password)) {
        puts("Current password is incorrect.");
        return false;
    }
    if (!read_line("New password: ", replacement, sizeof(replacement)) || replacement[0] == '\0') {
        puts("New password cannot be empty.");
        return false;
    }
    if (!read_line("Repeat new password: ", confirmation, sizeof(confirmation))) return false;
    if (strcmp(replacement, confirmation) != 0) {
        puts("New passwords do not match.");
        return false;
    }

    char password_hash[ATM_PASSWORD_LEN];
    hash_password(replacement, password_hash);
    if (storage_update_password(user->id, user->name, password_hash) != STORAGE_RESULT_OK) {
        puts("Could not save the new password.");
        return false;
    }
    snprintf(user->password, sizeof(user->password), "%s", password_hash);
    (void)storage_login_success(user->name);
    puts("Password changed successfully.");
    return true;
}
