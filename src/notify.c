#include "header.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

bool notification_start(const User *user, NotificationSession *session) {
    (void)user;
    session->active = false;
    session->child_pid = -1;
    session->fifo_path[0] = '\0';
    return false;
}

void notification_stop(NotificationSession *session) {
    session->active = false;
}

void notification_send(const char *username, const char *message) {
    (void)username;
    (void)message;
}

#else

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void fifo_path_for_user(const char *username, char path[ATM_PATH_LEN]) {
    char safe[ATM_NAME_LEN];
    size_t out = 0U;
    for (size_t i = 0U; username[i] != '\0' && out + 1U < sizeof(safe); ++i) {
        unsigned char ch = (unsigned char)username[i];
        safe[out++] = (char)(isalnum(ch) ? ch : '_');
    }
    safe[out] = '\0';
    snprintf(path, ATM_PATH_LEN, "/tmp/atm-management-%s.fifo", safe);
}

bool notification_start(const User *user, NotificationSession *session) {
    memset(session, 0, sizeof(*session));
    session->child_pid = -1;
    fifo_path_for_user(user->name, session->fifo_path);

    unlink(session->fifo_path);
    if (mkfifo(session->fifo_path, 0600) != 0) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        unlink(session->fifo_path);
        return false;
    }

    if (pid == 0) {
        for (;;) {
            int fd = open(session->fifo_path, O_RDONLY);
            if (fd < 0) {
                _exit(0);
            }

            char buffer[256];
            ssize_t read_count;
            while ((read_count = read(fd, buffer, sizeof(buffer) - 1U)) > 0) {
                buffer[read_count] = '\0';
                printf("\n[NOTIFICATION] %s\n", buffer);
                fflush(stdout);
            }
            close(fd);
        }
    }

    session->child_pid = (long)pid;
    session->active = true;
    return true;
}

void notification_stop(NotificationSession *session) {
    if (!session->active) {
        return;
    }

    if (session->child_pid > 0) {
        kill((pid_t)session->child_pid, SIGTERM);
    }
    unlink(session->fifo_path);
    session->active = false;
}

void notification_send(const char *username, const char *message) {
    char path[ATM_PATH_LEN];
    fifo_path_for_user(username, path);

    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        return;
    }

    size_t len = strlen(message);
    size_t offset = 0U;
    while (offset < len) {
        ssize_t written = write(fd, message + offset, len - offset);
        if (written > 0) {
            offset += (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    close(fd);
}

#endif
