# Module index

| Area | Files | Responsibility |
| --- | --- | --- |
| Entry point | `src/main.c` | Startup menu, login session lifecycle |
| Authentication | `src/auth.c`, `src/password.c` | Registration, login, SHA-256 password hashing |
| Account rules | `src/account.c` | Account types, dates, interest calculations |
| ATM actions | `src/system.c` | Create/update/check/list/transaction/remove/transfer flows |
| Persistence | `src/storage.c` | Text-file loading and atomic rewrites |
| Input helpers | `src/utils.c` | Safe line and numeric input parsing |
| Notifications | `src/notify.c` | Best-effort cross-process transfer notification |
| Shared API | `src/header.h` | Shared types, constants, function declarations |
| Audit coverage | `tests/` | Unit and end-to-end CLI checks |
