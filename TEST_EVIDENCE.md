# Verification evidence

This file maps the optional project features to concrete implementation and executable checks.

| Feature | Implementation | Evidence |
| --- | --- | --- |
| Instant transfer notification | POSIX FIFO + listener child process | `src/notify.c`, `tests/notification_flow.sh` |
| Updated terminal interface | TTY-aware ANSI colors, framed banner/session header and section layout | `src/ui.c`, `src/main.c`, `src/system.c`, `tests/bonus_flow.sh` |
| Encrypted password storage | SHA-256 hashes are stored instead of plaintext passwords | `src/password.c`, `src/auth.c`, `tests/core_flow.sh`, `tests/bonus_flow.sh` |
| Relational database | SQLite is the default backend; `users` and `accounts` are related by a foreign key and indexed by owner | `src/storage.c`, `tests/bonus_flow.sh` |
| Own Makefile | Warning-clean C11 build, verification, sanitizer target and optional text-only build | `Makefile` |
| More features | Authenticated password change and account summary dashboard | `src/auth.c`, `src/system.c`, `tests/bonus_flow.sh` |
| Optimized/refactored starter code | Shared validation, reusable account rules, storage abstraction, prepared SQLite statements, explicit transactions, DB constraints/indexes, atomic text fallback writes and separated modules | `src/*.c`, `agent/module-index.md` |

## One-command verification

```bash
make verify
```

The command runs:

```text
interest/date unit tests
named core functional cases against SQLite
SQLite schema/FK/index + TUI + password change + summary + text fallback
instant ownership-transfer notification with two active sessions
```

`make check` performs a clean rebuild before running the same verification suite.

Memory and undefined-behavior check for the core scenario:

```bash
make sanitize
```

## SQLite schema evidence

On first start the application creates `data/atm.db` and bootstraps it from the assignment-compatible seed text files when the database is empty.

The relational model contains:

```text
users
  id PRIMARY KEY
  name UNIQUE
  password

accounts
  id PRIMARY KEY
  user_id FOREIGN KEY -> users(id)
  account_number UNIQUE
  created
  country
  phone
  balance CHECK(balance >= 0)
  type CHECK(valid account type)
```

`idx_accounts_user_id` indexes account ownership lookups. SQLite writes use prepared statements and explicit transactions.

The legacy text backend remains available for compatibility:

```bash
ATM_STORAGE=text ./atm
```

A build without the SQLite dependency is also available:

```bash
make TEXT_ONLY=1
```
