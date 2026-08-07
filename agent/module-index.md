# Module index

| Area | Files | Responsibility |
| --- | --- | --- |
| Entry point | `src/main.c` | Startup menu and login session lifecycle |
| Terminal UI | `src/ui.c` | TTY-aware ANSI styling, banners and section headers |
| Authentication | `src/auth.c`, `src/password.c` | Registration, login, password change, SHA-256 hashing |
| Account rules | `src/account.c` | Account types, dates and interest calculations |
| ATM actions | `src/system.c` | Create/update/check/list/transaction/remove/transfer plus account summary |
| Persistence | `src/storage.c` | SQLite default backend, schema/bootstrap/indexes and text-file fallback |
| Input helpers | `src/utils.c` | Safe line and numeric input parsing, length and whitespace validation |
| Notifications | `src/notify.c` | Cross-process transfer notification through POSIX FIFO |
| Shared API | `src/header.h` | Shared types, constants and function declarations |
| Core verification | `tests/core_flow.sh`, `tests/edge_flow.sh`, `tests/test_interest.c`, `tests/verify.sh` | Named functional scenarios, boundary/rejection cases, interest rules and the single verification entry point |
| Optional features | `tests/bonus_flow.sh`, `tests/notification_flow.sh`, `TEST_EVIDENCE.md` | Executable and documented coverage for extra project features |
