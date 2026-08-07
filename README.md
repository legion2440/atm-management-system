# ATM Management System

A terminal ATM management application written in C. It implements the complete mandatory 01-edu assignment and all items from the official bonus section: relational storage, improved terminal UI, encrypted passwords, instant transfer notifications, an original Makefile, extra features, and a refactored/optimized codebase.

SQLite is the default runtime backend. The original text-file format is retained as a compatible fallback and as seed data for a fresh database.

· [Русская версия](README_RU.md)  
· [School repository](https://01.tomorrow-school.ai/git/nyestaye/atm-management-system)  
· [01-edu subject](https://github.com/01-edu/public/tree/master/subjects/atm-management-system)

## 📋 TOC

- [🚀 Quick start](#-quick-start)
- [📝 About](#-about)
- [✨ Features](#-features)
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
- Linux / WSL is recommended for the POSIX FIFO notification bonus

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

On the first start, `data/atm.db` is created and populated from the seed text files. Passwords are stored as SHA-256 hashes.

Reset all local sample data:

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
- SHA-256 password storage;
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
- persist the updated balance immediately.

## 🎁 Bonus coverage

Every item from the official bonus section has concrete evidence:

| Bonus | Status | Evidence |
| --- | --- | --- |
| Instant notification after ownership transfer | ✅ | POSIX FIFO listener, `tests/notification_flow.sh` |
| Updated terminal interface | ✅ | TTY-aware ANSI colors, framed menus and section headers in `src/ui.c` |
| Encrypted passwords | ✅ | SHA-256 in `src/password.c` |
| Relational database | ✅ | SQLite `users` + `accounts`, FK, constraints and index in `src/storage.c` |
| Own Makefile | ✅ | build/verify/sanitize/text-only targets |
| More features | ✅ | password change + account summary |
| Optimized starter code | ✅ | modular refactor, reusable validation/rules, prepared statements, transactions, indexes, atomic text fallback |

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
```

`idx_accounts_user_id` indexes ownership lookups. Database writes use prepared statements and explicit transactions.

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

The text backend uses temporary files plus `rename` for atomic replacement.

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

The suite reports named `[PASS]` checks from four layers:

- unit boundary checks for dates, leap years, maturity dates and interest calculations;
- 25 core functional cases for registration, login, account creation/update, interest, transactions, removals, ownership transfer and persistence;
- 20 edge cases for invalid credentials/input, duplicate or negative account numbers, invalid dates/types, negative balances, zero/negative transactions, invalid actions, missing/self transfer, former-owner access, password-change failures, whitespace tokens, oversized input and persistent-state integrity after rejected operations;
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
- SHA-256 satisfies the educational password-encryption bonus. A production authentication system should use a salted slow password KDF such as Argon2, scrypt, or bcrypt.
- Runtime SQLite files are ignored by Git and recreated from seed data after `scripts/reset_data.sh`.
- The FIFO notification bonus is POSIX-specific; the rest of the project is independent of it.

## 🧑‍💻 Author

- Nazar Yestayev (@nyestaye)
