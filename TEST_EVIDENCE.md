# Verification evidence

This file maps the optional project features and hardening work to concrete implementation and executable checks.

| Feature | Implementation | Evidence |
| --- | --- | --- |
| Instant transfer notification | POSIX FIFO + listener child process | `src/notify.c`, `tests/notification_flow.sh` |
| Updated terminal interface | TTY-aware ANSI colors, framed banner/session header and section layout | `src/ui.c`, `src/main.c`, `src/system.c`, `tests/bonus_flow.sh` |
| Protected password storage | Salted PBKDF2-HMAC-SHA256 with legacy credential migration | `src/password.c`, `src/auth.c`, `tests/security_flow.sh`, `tests/bonus_flow.sh` |
| Brute-force protection | Five-failure persisted lockout stored in SQLite `login_security` | `src/auth.c`, `src/storage.c`, `tests/security_flow.sh` |
| Relational database | SQLite default backend; `users` and `accounts` related by FK and indexed by owner | `src/storage.c`, `tests/bonus_flow.sh` |
| Atomic money updates | SQLite balance mutation inside `BEGIN IMMEDIATE` transactions | `src/storage.c`, `src/system.c`, `tests/test_concurrency.c` |
| Own Makefile | Warning-clean C11 build, verification, sanitizer target and optional text-only build | `Makefile` |
| More features | Authenticated password change and account summary dashboard | `src/auth.c`, `src/system.c`, `tests/bonus_flow.sh` |
| Optimized/refactored starter code | Shared validation, reusable account rules, storage abstraction, targeted prepared statements, explicit transactions, DB constraints/indexes and separated modules | `src/*.c`, `agent/module-index.md` |

## One-command verification

```bash
make verify
```

The command runs:

```text
interest/date unit tests
25 named core functional cases against SQLite
20 boundary and rejection cases
5 credential-security cases
2 multi-process SQLite concurrency cases
SQLite schema/FK/index + TUI + password change + summary + text fallback
instant ownership-transfer notification with two active sessions
```

Security coverage includes PBKDF2 migration from legacy credentials, different salts for equal passwords, lockout after five failures, lockout persistence across process restarts and successful login after lock state is cleared.

Boundary coverage includes wrong credentials, negative and duplicate account numbers, negative initial balance, invalid dates and account types, whitespace-sensitive fields, zero/negative transaction amounts, invalid transaction actions, missing/self ownership transfers, failed password changes, access by a former owner after transfer, oversized input, malformed dates and leap-day maturity.

Concurrency coverage starts multiple processes against one SQLite database and proves that concurrent deposits are all retained and that two simultaneous withdrawals cannot both overdraw the same balance.

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

login_security
  name PRIMARY KEY
  failed_attempts CHECK(failed_attempts >= 0)
  locked_until CHECK(locked_until >= 0)
```

`idx_accounts_user_id` indexes account ownership lookups. SQLite writes use bound parameters. Account create/update/delete/transfer use targeted statements, and balance-changing transactions hold a SQLite write transaction across read/validation/update so concurrent processes cannot overwrite a stale balance.

The legacy text backend remains available for compatibility:

```bash
ATM_STORAGE=text ./atm
```

It keeps atomic file replacement and login lockout state, but multi-writer balance guarantees apply to the default SQLite backend.

A build without the SQLite dependency is also available:

```bash
make TEXT_ONLY=1
```
