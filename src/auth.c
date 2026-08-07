#include "header.h"

#include <stdio.h>
#include <string.h>

bool find_user_by_name(const char *name, User *user) {
    User users[ATM_MAX_USERS];
    size_t count;
    if (!load_users(users, ATM_MAX_USERS, &count)) {
        return false;
    }

    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            if (user != NULL) {
                *user = users[i];
            }
            return true;
        }
    }
    return false;
}

bool register_user_interactive(void) {
    User users[ATM_MAX_USERS];
    size_t count;
    if (!load_users(users, ATM_MAX_USERS, &count)) {
        return false;
    }

    char name[ATM_NAME_LEN];
    char password[ATM_PASSWORD_LEN];
    if (!read_line("Username: ", name, sizeof(name)) || !read_line("Password: ", password, sizeof(password))) {
        return false;
    }

    if (name[0] == '\0' || password[0] == '\0' || strchr(name, ' ') != NULL) {
        puts("Username and password must be non-empty; usernames cannot contain spaces.");
        return false;
    }

    int max_id = -1;
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            printf("User '%s' already exists.\n", name);
            return false;
        }
        if (users[i].id > max_id) {
            max_id = users[i].id;
        }
    }

    if (count >= ATM_MAX_USERS) {
        puts("User storage is full.");
        return false;
    }

    User new_user;
    new_user.id = max_id + 1;
    snprintf(new_user.name, sizeof(new_user.name), "%s", name);
    hash_password(password, new_user.password);
    users[count++] = new_user;

    if (!save_users(users, count)) {
        puts("Registration failed while saving data.");
        return false;
    }

    printf("User '%s' registered successfully.\n", name);
    return true;
}

bool login_user_interactive(User *user) {
    User users[ATM_MAX_USERS];
    size_t count;
    if (!load_users(users, ATM_MAX_USERS, &count)) {
        return false;
    }

    char name[ATM_NAME_LEN];
    char password[ATM_PASSWORD_LEN];
    if (!read_line("Username: ", name, sizeof(name)) || !read_line("Password: ", password, sizeof(password))) {
        return false;
    }

    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(users[i].name, name) == 0 && password_matches(password, users[i].password)) {
            *user = users[i];
            printf("Welcome, %s.\n", user->name);
            return true;
        }
    }

    puts("Invalid username or password.");
    return false;
}
