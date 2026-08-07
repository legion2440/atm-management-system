#include "header.h"

#include <stdio.h>

int main(void) {
    if (!ensure_data_files()) {
        return 1;
    }

    puts("============================");
    puts("      ATM MANAGEMENT");
    puts("============================");

    for (;;) {
        puts("\n1. Register");
        puts("2. Login");
        puts("3. Exit");

        int choice;
        if (!prompt_int("Choice: ", &choice)) {
            putchar('\n');
            return 0;
        }

        if (choice == 1) {
            (void)register_user_interactive();
        } else if (choice == 2) {
            User user;
            if (login_user_interactive(&user)) {
                NotificationSession notifications;
                (void)notification_start(&user, &notifications);
                account_menu(&user, &notifications);
                notification_stop(&notifications);
            }
        } else if (choice == 3) {
            puts("Goodbye.");
            return 0;
        } else {
            puts("Invalid choice.");
        }
    }
}
