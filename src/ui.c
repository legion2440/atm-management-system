#include "header.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#define ATM_ISATTY(fd) _isatty(fd)
#define ATM_STDOUT_FD _fileno(stdout)
#else
#include <unistd.h>
#define ATM_ISATTY(fd) isatty(fd)
#define ATM_STDOUT_FD STDOUT_FILENO
#endif

static bool color_enabled(void) {
    const char *disabled = getenv("NO_COLOR");
    const char *atm_disabled = getenv("ATM_NO_COLOR");
    return disabled == NULL && atm_disabled == NULL && ATM_ISATTY(ATM_STDOUT_FD) != 0;
}

void ui_banner(void) {
    if (color_enabled()) fputs("\033[1;36m", stdout);
    puts("+==========================================+");
    puts("|          ATM MANAGEMENT SYSTEM           |");
    puts("+==========================================+");
    if (color_enabled()) fputs("\033[0m", stdout);
    printf("Storage: %s\n", storage_backend_name());
}

void ui_session_header(const User *user) {
    if (color_enabled()) fputs("\033[1;34m", stdout);
    puts("\n+------------------------------------------+");
    printf("| User: %-34s |\n", user->name);
    printf("| Storage: %-31s |\n", storage_backend_name());
    puts("+------------------------------------------+");
    if (color_enabled()) fputs("\033[0m", stdout);
}

void ui_section(const char *title) {
    if (color_enabled()) fputs("\033[1;33m", stdout);
    printf("\n-- %s --\n", title);
    if (color_enabled()) fputs("\033[0m", stdout);
}
