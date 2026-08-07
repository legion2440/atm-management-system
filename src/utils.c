#include "header.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void trim_whitespace(char *text) {
    if (text == NULL) {
        return;
    }

    char *start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    size_t len = strlen(text);
    while (len > 0U && isspace((unsigned char)text[len - 1U])) {
        text[--len] = '\0';
    }
}

bool contains_whitespace(const char *text) {
    if (text == NULL) {
        return false;
    }
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        if (isspace((unsigned char)text[i])) {
            return true;
        }
    }
    return false;
}

bool read_line(const char *prompt, char *buffer, size_t size) {
    if (buffer == NULL || size < 2U) {
        return false;
    }

    if (prompt != NULL) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return false;
    }

    if (strchr(buffer, '\n') == NULL) {
        int ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
            puts("Input is too long.");
            buffer[0] = '\0';
            return false;
        }
    }

    trim_whitespace(buffer);
    return true;
}

static bool parse_long_value(const char *text, long long *value) {
    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    *value = parsed;
    return true;
}

bool prompt_int(const char *prompt, int *value) {
    char buffer[64];
    long long parsed;

    for (;;) {
        if (!read_line(prompt, buffer, sizeof(buffer))) {
            return false;
        }
        if (parse_long_value(buffer, &parsed) && parsed >= -2147483647LL - 1LL && parsed <= 2147483647LL) {
            *value = (int)parsed;
            return true;
        }
        puts("Invalid number. Please try again.");
    }
}

bool prompt_long_long(const char *prompt, long long *value) {
    char buffer[64];

    for (;;) {
        if (!read_line(prompt, buffer, sizeof(buffer))) {
            return false;
        }
        if (parse_long_value(buffer, value)) {
            return true;
        }
        puts("Invalid number. Please try again.");
    }
}

bool prompt_double(const char *prompt, double *value) {
    char buffer[64];

    for (;;) {
        if (!read_line(prompt, buffer, sizeof(buffer))) {
            return false;
        }

        char *end = NULL;
        errno = 0;
        double parsed = strtod(buffer, &end);
        if (errno == 0 && end != buffer && *end == '\0' && isfinite(parsed)) {
            *value = parsed;
            return true;
        }
        puts("Invalid amount. Please try again.");
    }
}
