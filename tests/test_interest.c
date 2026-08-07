#include "header.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void assert_money(double actual, double expected) {
    assert(fabs(actual - expected) < 0.005);
}

int main(void) {
    Account account = {0};
    account.created = (Date){10, 10, 2012};
    account.balance = 1001.20;

    account.type = ACCOUNT_CURRENT;
    assert_money(account_interest(&account), 0.0);

    account.type = ACCOUNT_SAVINGS;
    assert_money(account_interest(&account), 5.8403333333);

    account.type = ACCOUNT_FIXED01;
    assert_money(account_interest(&account), 40.048);
    Date maturity = account_interest_date(&account);
    assert(maturity.day == 10 && maturity.month == 10 && maturity.year == 2013);

    account.type = ACCOUNT_FIXED02;
    assert_money(account_interest(&account), 100.12);
    maturity = account_interest_date(&account);
    assert(maturity.year == 2014);

    account.type = ACCOUNT_FIXED03;
    assert_money(account_interest(&account), 240.288);
    maturity = account_interest_date(&account);
    assert(maturity.year == 2015);

    assert(account_type_from_string("saving") == ACCOUNT_SAVINGS);
    assert(account_type_from_string("savings") == ACCOUNT_SAVINGS);
    assert(account_type_from_string("unknown") == ACCOUNT_INVALID);
    assert(strcmp(account_type_name(ACCOUNT_FIXED03), "fixed03") == 0);

    Date leap;
    assert(parse_date("29/02/2024", &leap));
    assert(!parse_date("29/02/2023", &leap));
    assert(!parse_date("31/04/2024", &leap));
    assert(!parse_date("00/01/2024", &leap));
    assert(!parse_date("01/13/2024", &leap));
    assert(!parse_date("01-01-2024", &leap));

    account.created = (Date){29, 2, 2024};
    account.type = ACCOUNT_FIXED01;
    maturity = account_interest_date(&account);
    assert(maturity.day == 28 && maturity.month == 2 && maturity.year == 2025);

    puts("interest/date tests: OK");
    return 0;
}
