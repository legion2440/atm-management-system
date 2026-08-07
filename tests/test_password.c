#include "header.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *known =
        "pbkdf2-sha256$100000$30313233343536373839616263646566$"
        "a75190a792cd59d6d9c8c3a63b11c276ad449972b7886e1c2d819c286053366f";

    assert(password_matches("password", known));
    assert(!password_matches("not-password", known));
    assert(!password_needs_upgrade(known));
    assert(password_needs_upgrade("sha256:legacy"));
    assert(password_needs_upgrade("plaintext"));

    char first[ATM_PASSWORD_LEN];
    char second[ATM_PASSWORD_LEN];
    hash_password("same-password", first);
    hash_password("same-password", second);
    assert(strcmp(first, second) != 0);
    assert(password_matches("same-password", first));
    assert(password_matches("same-password", second));

    puts("password KDF tests: OK");
    return 0;
}
