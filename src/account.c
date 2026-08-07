#include "header.h"

#include <stdio.h>
#include <string.h>

static bool is_leap_year(int year) {
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

bool date_is_valid(Date date) {
    if (date.year < 1900 || date.year > 9999 || date.month < 1 || date.month > 12 || date.day < 1) {
        return false;
    }

    static const int days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days_per_month[date.month - 1];
    if (date.month == 2 && is_leap_year(date.year)) {
        limit = 29;
    }
    return date.day <= limit;
}

bool parse_date(const char *text, Date *date) {
    int day;
    int month;
    int year;
    char tail;
    if (sscanf(text, "%d/%d/%d%c", &day, &month, &year, &tail) != 3) {
        return false;
    }

    Date parsed = {day, month, year};
    if (!date_is_valid(parsed)) {
        return false;
    }
    *date = parsed;
    return true;
}

AccountType account_type_from_string(const char *text) {
    if (strcmp(text, "current") == 0) {
        return ACCOUNT_CURRENT;
    }
    if (strcmp(text, "saving") == 0 || strcmp(text, "savings") == 0) {
        return ACCOUNT_SAVINGS;
    }
    if (strcmp(text, "fixed01") == 0) {
        return ACCOUNT_FIXED01;
    }
    if (strcmp(text, "fixed02") == 0) {
        return ACCOUNT_FIXED02;
    }
    if (strcmp(text, "fixed03") == 0) {
        return ACCOUNT_FIXED03;
    }
    return ACCOUNT_INVALID;
}

const char *account_type_name(AccountType type) {
    switch (type) {
        case ACCOUNT_CURRENT:
            return "current";
        case ACCOUNT_SAVINGS:
            return "savings";
        case ACCOUNT_FIXED01:
            return "fixed01";
        case ACCOUNT_FIXED02:
            return "fixed02";
        case ACCOUNT_FIXED03:
            return "fixed03";
        default:
            return "invalid";
    }
}

double account_interest(const Account *account) {
    switch (account->type) {
        case ACCOUNT_SAVINGS:
            return account->balance * 0.07 / 12.0;
        case ACCOUNT_FIXED01:
            return account->balance * 0.04;
        case ACCOUNT_FIXED02:
            return account->balance * 0.05 * 2.0;
        case ACCOUNT_FIXED03:
            return account->balance * 0.08 * 3.0;
        default:
            return 0.0;
    }
}

Date account_interest_date(const Account *account) {
    Date result = account->created;
    switch (account->type) {
        case ACCOUNT_FIXED01:
            result.year += 1;
            break;
        case ACCOUNT_FIXED02:
            result.year += 2;
            break;
        case ACCOUNT_FIXED03:
            result.year += 3;
            break;
        default:
            break;
    }

    if (result.month == 2 && result.day == 29 && !is_leap_year(result.year)) {
        result.day = 28;
    }
    return result;
}
