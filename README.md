# ATM Management System

A terminal ATM management application written in C. It implements the complete mandatory 01-edu assignment and all items from the official bonus section: relational storage, improved terminal UI, protected password storage, instant transfer notifications, an original Makefile, extra features, and a refactored/optimized codebase.

SQLite is the default runtime backend. The original text-file format is retained as a compatible fallback and as seed data for a fresh database.

· [Русская версия](README_RU.md)  
· [School repository](https://01.tomorrow-school.ai/git/nyestaye/atm-management-system)  
· [01-edu subject](https://github.com/01-edu/public/tree/master/subjects/atm-management-system)

## 📋 TOC

- [🚀 Quick start](#-quick-start)
- [📝 About](#-about)
- [✨ Features](#-features)
- [🔐 Security hardening](#-security-hardening)
- [🎁 Bonus coverage](#-bonus-coverage)
- [💰 Interest rules](#-interest-rules)
- [💾 Storage](#-storage)
- [🔔 Transfer notifications](#-transfer-notifications)
- [🧪 Tests and verification](#-tests-and-verification)
- [📁 Project structure](#-project-structure)
- [⚠️ Notes](#️-notes)
- [🧑‍💻 Author](#-author)

## 🚀 Quick start

### Requirements

- GCC or Clang with C11 support
- GNU Make
- SQLite development library
- Bash and Python 3 for the automated verification scripts
- Linux / WSL is recommended for the POSIX FIFO notification and concurrency checks

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev python3
```

### Clone

```bash
git clone https://01.tomorrow-school.ai/git/nyestaye/atm-management-system
cd atm-management-system
```

### Build

```bash
make
```

### Run

```bash
./atm
```

The repository contains two seed users:

| User | Password |
| --- | --- |
| `Alice` | `1234password` |
| `Michel` | `password1234` |

On first start, `data/atm.db` is created and populated from the seed text files. Legacy seed credentials are accepted for compatibility and automatically upgraded to salted PBKDF2 after a successful login.

Reset all local sample data and login-security state:

```bash
bash scripts/reset_data.sh
```

## 📝 About

After login, a user can manage only their own accounts. Mandatory menu entries keep the same numeric positions used by the official evaluation sequence:

1. Create a new account
2. Update account information
3. Check one account and interest
4. List owned accounts
5. Make a transaction
6. Remove an account
7. Transfer ownership
8. Logout
9. Change password `[bonus]`
10. Account summary `[bonus]`

The implementation is split into small modules for account rules, authentication, storage, input handling, notifications, UI, and session actions rather than keeping all starter logic in a few large functions.

## ✨ Features

### Authentication

- register a new user;
- reject duplicate usernames;
- login using persisted credentials;
- salted PBKDF2-HMAC-SHA256 password storage;
- automatic migration of legacy SHA-256/plaintext seed credentials after successful login;
- persistent per-username lockout after repeated failed login attempts;
- change password after verifying the current password.

### Accounts

- `current`, `saving` / `savings`, `fixed01`, `fixed02`, `fixed03`;
- globally unique account numbers;
- update country or phone number;
- inspect one owned account;
- list all owned accounts;
- remove an account;
- transfer ownership to another registered user;
- account summary with total balance and counts by account class.

### Transactions

- deposit into `current` and `savings`;
- withdraw from `current` and `savings`;
- reject zero or negative amounts;
- reject withdrawals above the available balance;
- reject all transactions on fixed accounts;
- serialize SQLite balance changes inside `BEGIN IMMEDIATE` transactions so concurrent writers cannot overwrite each other's balances.

## 🔐 Security hardening

The project includes several protections beyond the minimum functional requirements:

- **Password KDF:** new passwords use `PBKDF2-HMAC-SHA256` with 100,000 iterations and an individual 128-bit salt. Equal passwords therefore produce different stored values.
- **Credential migration:** legacy `sha256:` and plaintext seed records remain readable only for compatibility; a successful login replaces them with the PBKDF2 format.
- **Brute-force protection:** five failed attempts for the same username create a 30-second lockout. The state is stored in SQLite, so restarting the program does not bypass the lock. `ATM_LOGIN_LOCK_SECONDS` exists for deterministic testing and controlled deployments.
- **Authorization:** account mutations use both the authenticated `user_id` and account number, preventing operations on another user's account by number alone.
- **SQL injection resistance:** SQLite writes use prepared statements and bound values rather than concatenating user input into SQL.
- **Concurrent money safety:** deposits and withdrawals read, validate and update the balance while holding a SQLite write transaction. A multi-process test verifies that concurrent deposits are not lost and concurrent withdrawals cannot overdraw the account.

## 🎁 Bonus coverage

Every item from the official bonus section has concrete evidence:

| Bonus | Status | Evidence |
| --- | --- | --- |
| Instant notification after ownership transfer | ✅ | POSIX FIFO listener, `tests/notification_flow.sh` |
| Updated terminal interface | ✅ | TTY-aware ANSI colors, framed menus and section headers in `src/ui.c` |
| Protected passwords | ✅ | salted PBKDF2 in `src/password.c`, migration and lockout in `src/auth.c` |
| Relational database | ✅ | SQLite `users` + `accounts`, FK, constraints and index in `src/storage.c` |
| Own Makefile | ✅ | build/verify/sanitize/text-only targets |
| More features | ✅ | password change + account summary |
| Optimized starter code | ✅ | modular refactor, prepared statements, targeted writes, explicit transactions, indexes and reusable validation/rules |

Detailed mapping: [`TEST_EVIDENCE.md`](TEST_EVIDENCE.md).

## 💰 Interest rules

The calculations match the exact reference values from the official checklist.

| Type | Rule | Reference for `$1001.20`, created `10/10/2012` |
| --- | --- | --- |
| `current` | no interest | no-interest message |
| `savings` | 7% yearly, paid monthly | `$5.84` on day 10 every month |
| `fixed01` | 4% × 1 year | `$40.05` on `10/10/2013` |
| `fixed02` | 5% × 2 years | `$100.12` on `10/10/2014` |
| `fixed03` | 8% × 3 years | `$240.29` on `10/10/2015` |

The date and interest rules live in `src/account.c` and are shared by the CLI and tests.

## 💾 Storage

### SQLite — default

The application creates:

```text
data/atm.db
```

Relational model:

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
  type CHECK(valid type)

login_security
  name PRIMARY KEY
  failed_attempts CHECK(failed_attempts >= 0)
  locked_until CHECK(locked_until >= 0)
```

`idx_accounts_user_id` indexes ownership lookups. User/account writes use prepared statements. Balance changes use explicit write transactions, while account create/update/delete/transfer operations use targeted SQL instead of rewriting the whole account table.

When the database is empty, the application imports the assignment-compatible seed files:

```text
data/users.txt
data/records.txt
```

### Text fallback

The original text storage remains usable:

```bash
ATM_STORAGE=text ./atm
```

A dependency-free text-only build is also available:

```bash
make fclean
make TEXT_ONLY=1
```

The text backend keeps atomic file replacement and persistent login lockout state, but it is a compatibility backend and is not intended for multiple concurrent writers. Multi-process balance guarantees apply to the default SQLite backend.

## 🔔 Transfer notifications

On POSIX, login creates a per-user FIFO in `/tmp` and starts a listener child process. If another active session transfers an account to that user, the receiver immediately sees a message such as:

```text
[NOTIFICATION] You received account 777 from Alice.
```

This bonus is tested with two concurrent application sessions in `tests/notification_flow.sh`.

Native Windows keeps ownership transfer itself but disables the POSIX FIFO listener. Use Linux or WSL to demonstrate this bonus.

## 🧪 Tests and verification

For the evaluator, the shortest path is one command:

```bash
make verify
```

The suite reports named `[PASS]` checks from these layers:

- unit boundary checks for dates, leap years, maturity dates and interest calculations;
- 25 core functional cases for registration, login, account creation/update, interest, transactions, removals, ownership transfer and persistence;
- 20 edge cases for invalid credentials/input, duplicate or negative account numbers, invalid dates/types, negative balances, zero/negative transactions, invalid actions, missing/self transfer, former-owner access, password-change failures, whitespace tokens, oversized input and persistent-state integrity after rejected operations;
- 5 credential-security cases covering PBKDF2 migration, unique salts, persisted lockout and post-lock recovery;
- 2 multi-process concurrency cases proving no lost deposits and no concurrent overdraft on SQLite;
- optional-feature checks for SQLite schema/FK/index, password storage, TUI, account summary, text fallback and instant cross-session notification.

A clean rebuild plus the same suite:

```bash
make check
```

Compiler flags:

```text
-std=c11 -Wall -Wextra -Werror -pedantic
```

ASan + UBSan core-flow check:

```bash
make sanitize
```

CI also verifies that the project still builds without SQLite:

```bash
make TEXT_ONLY=1
```

## 📁 Project structure

```text
atm-management-system/
├── .github/
│   └── workflows/
│       └── ci.yml
├── agent/
│   └── module-index.md
├── data/
│   ├── records.txt
│   └── users.txt
├── scripts/
│   └── reset_data.sh
├── src/
│   ├── account.c
│   ├── auth.c
│   ├── header.h
│   ├── main.c
│   ├── notify.c
│   ├── password.c
│   ├── storage.c
│   ├── system.c
│   ├── ui.c
│   └── utils.c
├── tests/
│   ├── fixtures/
│   ├── bonus_flow.sh
│   ├── core_flow.sh
│   ├── edge_flow.sh
│   ├── notification_flow.sh
│   ├── security_flow.sh
│   ├── test_concurrency.c
│   ├── test_interest.c
│   └── verify.sh
├── AGENTS.md
├── Makefile
├── README.md
├── README_RU.md
└── TEST_EVIDENCE.md
```

## ⚠️ Notes

- SQLite is the default runtime backend; `users.txt` and `records.txt` are seed/fallback storage, not the primary data after startup.
- PBKDF2 is implemented locally to keep the school project dependency-light. A production system would normally prefer a memory-hard KDF such as Argon2id and tune its cost to the deployment hardware.
- The lockout is deliberately simple: five failures and 30 seconds. Real banking authentication would normally add broader rate limiting, monitoring, device/session controls and stronger operational protections.
- Runtime SQLite and text-mode login-security files are ignored by Git and recreated after `scripts/reset_data.sh`.
- The FIFO notification bonus is POSIX-specific; the rest of the project is independent of it.

## 🧑‍💻 Author

- Nazar Yestayev (@nyestaye)
