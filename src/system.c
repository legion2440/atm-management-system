#include "header.h"

#include <stdio.h>
#include <string.h>

static int next_account_id(const Account *accounts, size_t count) {
    int max_id = -1;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].id > max_id) {
            max_id = accounts[i].id;
        }
    }
    return max_id + 1;
}

static long find_owned_account(const Account *accounts, size_t count, const User *user, long long number) {
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id == user->id && accounts[i].number == number) {
            return (long)i;
        }
    }
    return -1;
}

static bool account_number_exists(const Account *accounts, size_t count, long long number) {
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].number == number) {
            return true;
        }
    }
    return false;
}

static void print_account(const Account *account) {
    printf("Account number: %lld\n", account->number);
    printf("Owner: %s\n", account->owner);
    printf("Created: %02d/%02d/%04d\n", account->created.day, account->created.month, account->created.year);
    printf("Country: %s\n", account->country);
    printf("Phone: %s\n", account->phone);
    printf("Balance: $%.2f\n", account->balance);
    printf("Type: %s\n", account_type_name(account->type));
}

static void print_interest(const Account *account) {
    if (account->type == ACCOUNT_CURRENT) {
        puts("You will not get interests because the account is of type current");
        return;
    }

    double interest = account_interest(account);
    if (account->type == ACCOUNT_SAVINGS) {
        printf("You will get $%.2f as interest on day %d of every month\n", interest, account->created.day);
        return;
    }

    Date date = account_interest_date(account);
    printf("You will get $%.2f as interest on %02d/%02d/%04d\n", interest, date.day, date.month, date.year);
}

static void create_account(const User *user) {
    ui_section("Create account");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }
    if (count >= ATM_MAX_ACCOUNTS) {
        puts("Account storage is full.");
        return;
    }

    Account account;
    account.id = next_account_id(accounts, count);
    account.user_id = user->id;
    snprintf(account.owner, sizeof(account.owner), "%s", user->name);

    if (!prompt_long_long("Account number: ", &account.number)) {
        return;
    }
    if (account.number < 0 || account_number_exists(accounts, count, account.number)) {
        puts("This account number already exists or is invalid.");
        return;
    }

    char date_text[32];
    for (;;) {
        if (!read_line("Creation date (dd/mm/yyyy): ", date_text, sizeof(date_text))) {
            return;
        }
        if (parse_date(date_text, &account.created)) {
            break;
        }
        puts("Invalid date. Use dd/mm/yyyy.");
    }

    if (!read_line("Country: ", account.country, sizeof(account.country)) || account.country[0] == '\0' || strchr(account.country, ' ') != NULL) {
        puts("Country must be a non-empty single token.");
        return;
    }
    if (!read_line("Phone number: ", account.phone, sizeof(account.phone)) || account.phone[0] == '\0' || strchr(account.phone, ' ') != NULL) {
        puts("Phone number must be non-empty and contain no spaces.");
        return;
    }
    if (!prompt_double("Initial deposit: $", &account.balance)) {
        return;
    }
    if (account.balance < 0.0) {
        puts("Initial deposit cannot be negative.");
        return;
    }

    char type_text[ATM_TYPE_LEN];
    for (;;) {
        if (!read_line("Account type (current/saving/fixed01/fixed02/fixed03): ", type_text, sizeof(type_text))) {
            return;
        }
        account.type = account_type_from_string(type_text);
        if (account.type != ACCOUNT_INVALID) {
            break;
        }
        puts("Invalid account type.");
    }

    accounts[count++] = account;
    if (!save_accounts(accounts, count)) {
        puts("Could not save the account.");
        return;
    }
    printf("Account %lld created successfully.\n", account.number);
}

static void update_account(const User *user) {
    ui_section("Update account");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    long long number;
    if (!prompt_long_long("Account number to update: ", &number)) {
        return;
    }
    long index = find_owned_account(accounts, count, user, number);
    if (index < 0) {
        puts("Account does not exist for this user.");
        return;
    }

    puts("1. Update phone number");
    puts("2. Update country");
    int choice;
    if (!prompt_int("Choice: ", &choice)) {
        return;
    }

    if (choice == 1) {
        if (!read_line("New phone number: ", accounts[index].phone, sizeof(accounts[index].phone)) || accounts[index].phone[0] == '\0' || strchr(accounts[index].phone, ' ') != NULL) {
            puts("Invalid phone number.");
            return;
        }
    } else if (choice == 2) {
        if (!read_line("New country: ", accounts[index].country, sizeof(accounts[index].country)) || accounts[index].country[0] == '\0' || strchr(accounts[index].country, ' ') != NULL) {
            puts("Invalid country.");
            return;
        }
    } else {
        puts("Invalid choice.");
        return;
    }

    if (!save_accounts(accounts, count)) {
        puts("Could not save the update.");
        return;
    }
    puts("Account information updated successfully.");
}

static void check_account(const User *user) {
    ui_section("Account details");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    long long number;
    if (!prompt_long_long("Account number to check: ", &number)) {
        return;
    }
    long index = find_owned_account(accounts, count, user, number);
    if (index < 0) {
        puts("Account does not exist for this user.");
        return;
    }

    print_account(&accounts[index]);
    print_interest(&accounts[index]);
}

static void list_accounts(const User *user) {
    ui_section("Owned accounts");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    bool found = false;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id == user->id) {
            if (found) {
                puts("------------------------------------------");
            }
            print_account(&accounts[i]);
            found = true;
        }
    }
    if (!found) {
        puts("You do not own any accounts.");
    }
}

static void account_summary(const User *user) {
    ui_section("Account summary");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    size_t owned = 0U;
    size_t current = 0U;
    size_t savings = 0U;
    size_t fixed = 0U;
    double total = 0.0;
    for (size_t i = 0U; i < count; ++i) {
        if (accounts[i].user_id != user->id) {
            continue;
        }
        owned++;
        total += accounts[i].balance;
        if (accounts[i].type == ACCOUNT_CURRENT) {
            current++;
        } else if (accounts[i].type == ACCOUNT_SAVINGS) {
            savings++;
        } else {
            fixed++;
        }
    }

    printf("Accounts: %zu\n", owned);
    printf("Total balance: $%.2f\n", total);
    printf("Current: %zu | Savings: %zu | Fixed: %zu\n", current, savings, fixed);
}

static bool transactions_allowed(AccountType type) {
    return type == ACCOUNT_CURRENT || type == ACCOUNT_SAVINGS;
}

static void make_transaction(const User *user) {
    ui_section("Transaction");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    long long number;
    if (!prompt_long_long("Account number: ", &number)) {
        return;
    }
    long index = find_owned_account(accounts, count, user, number);
    if (index < 0) {
        puts("Account does not exist for this user.");
        return;
    }
    if (!transactions_allowed(accounts[index].type)) {
        puts("Transactions are not allowed for fixed accounts.");
        return;
    }

    puts("1. Deposit");
    puts("2. Withdraw");
    int choice;
    if (!prompt_int("Choice: ", &choice)) {
        return;
    }

    double amount;
    if (!prompt_double("Amount: $", &amount)) {
        return;
    }
    if (amount <= 0.0) {
        puts("Transaction amount must be positive.");
        return;
    }

    if (choice == 1) {
        accounts[index].balance += amount;
    } else if (choice == 2) {
        if (amount > accounts[index].balance) {
            puts("Withdrawal denied: amount exceeds the available balance.");
            return;
        }
        accounts[index].balance -= amount;
    } else {
        puts("Invalid choice.");
        return;
    }

    if (!save_accounts(accounts, count)) {
        puts("Could not save the transaction.");
        return;
    }
    printf("Transaction completed. New balance: $%.2f\n", accounts[index].balance);
}

static void remove_account(const User *user) {
    ui_section("Remove account");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    long long number;
    if (!prompt_long_long("Account number to remove: ", &number)) {
        return;
    }
    long index = find_owned_account(accounts, count, user, number);
    if (index < 0) {
        puts("Account does not exist for this user.");
        return;
    }

    for (size_t i = (size_t)index + 1U; i < count; ++i) {
        accounts[i - 1U] = accounts[i];
    }
    count--;

    if (!save_accounts(accounts, count)) {
        puts("Could not remove the account.");
        return;
    }
    printf("Account %lld removed successfully.\n", number);
}

static void transfer_owner(const User *user) {
    ui_section("Transfer ownership");
    Account accounts[ATM_MAX_ACCOUNTS];
    size_t count;
    if (!load_accounts(accounts, ATM_MAX_ACCOUNTS, &count)) {
        return;
    }

    long long number;
    if (!prompt_long_long("Account number to transfer: ", &number)) {
        return;
    }
    long index = find_owned_account(accounts, count, user, number);
    if (index < 0) {
        puts("Account does not exist for this user.");
        return;
    }

    char target_name[ATM_NAME_LEN];
    if (!read_line("New owner's username: ", target_name, sizeof(target_name))) {
        return;
    }

    User target;
    if (!find_user_by_name(target_name, &target)) {
        puts("Target user does not exist.");
        return;
    }
    if (target.id == user->id) {
        puts("The account already belongs to this user.");
        return;
    }

    accounts[index].user_id = target.id;
    snprintf(accounts[index].owner, sizeof(accounts[index].owner), "%s", target.name);
    if (!save_accounts(accounts, count)) {
        puts("Could not transfer the account.");
        return;
    }

    printf("Account %lld transferred to %s successfully.\n", number, target.name);
    char message[256];
    snprintf(message, sizeof(message), "You received account %lld from %s.", number, user->name);
    notification_send(target.name, message);
}

void account_menu(User *user, NotificationSession *notifications) {
    (void)notifications;
    for (;;) {
        ui_session_header(user);
        puts("1. Create a new account");
        puts("2. Update information of account");
        puts("3. Check account details");
        puts("4. Check list of owned accounts");
        puts("5. Make transaction");
        puts("6. Remove existing account");
        puts("7. Transfer owner");
        puts("8. Logout");
        puts("9. Change password [bonus]");
        puts("10. Account summary [bonus]");

        int choice;
        if (!prompt_int("Choice: ", &choice)) {
            return;
        }

        switch (choice) {
            case 1:
                create_account(user);
                break;
            case 2:
                update_account(user);
                break;
            case 3:
                check_account(user);
                break;
            case 4:
                list_accounts(user);
                break;
            case 5:
                make_transaction(user);
                break;
            case 6:
                remove_account(user);
                break;
            case 7:
                transfer_owner(user);
                break;
            case 8:
                return;
            case 9:
                ui_section("Change password");
                (void)change_password_interactive(user);
                break;
            case 10:
                account_summary(user);
                break;
            default:
                puts("Invalid choice.");
                break;
        }
    }
}
