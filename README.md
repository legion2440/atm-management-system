# ATM Management System

A terminal-based ATM management application written in C. It implements the complete mandatory 01-edu assignment: user registration and login, account creation and management, interest calculation, transactions, account removal, and ownership transfer.

The project keeps the assignment-compatible text-file storage, hashes passwords with SHA-256, and adds instant transfer notifications between two active POSIX terminals as a bonus.

· [Русская версия](README_RU.md)  
· [School repository](https://01.tomorrow-school.ai/git/nyestaye/atm-management-system)  
· [01-edu subject](https://github.com/01-edu/public/tree/master/subjects/atm-management-system)

## 📋 TOC

- [🚀 Quick start](#-quick-start)
- [📝 About](#-about)
- [✨ Features](#-features)
- [💰 Interest rules](#-interest-rules)
- [💾 Data storage](#-data-storage)
- [🔔 Transfer notifications](#-transfer-notifications)
- [🧪 Tests and audit](#-tests-and-audit)
- [📁 Project structure](#-project-structure)
- [⚠️ Notes](#️-notes)
- [🧑‍💻 Author](#-author)

## 🚀 Quick start

### Requirements

- GCC or Clang with C11 support
- GNU Make
- Bash for the test scripts
- Linux / WSL is recommended for the POSIX notification bonus

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

On native Windows with MinGW/Git Bash the executable may be named `atm.exe`.

The repository starts with two sample users:

| User | Password |
| --- | --- |
| `Alice` | `1234password` |
| `Michel` | `password1234` |

Their passwords are stored as SHA-256 hashes, not as plaintext.

To restore the sample data after manual testing:

```bash
bash scripts/reset_data.sh
```

## 📝 About

The application follows the original ATM management assignment and uses a simple terminal interface. After login, a user can create and manage only their own accounts. Every mutating action is persisted immediately to the storage files.

The main session menu contains:

1. Create a new account
2. Update account information
3. Check one account and its interest
4. List owned accounts
5. Deposit or withdraw money
6. Remove an account
7. Transfer account ownership
8. Logout

Account numbers are globally unique. Country and phone fields use single whitespace-free tokens because the original assignment storage format is whitespace-separated.

## ✨ Features

### Authentication

- register new users;
- reject duplicate usernames;
- login with saved credentials;
- SHA-256 password storage;
- compatibility with legacy plaintext password rows if such data is imported.

### Account management

- create `current`, `saving` / `savings`, `fixed01`, `fixed02`, and `fixed03` accounts;
- update only the phone number or country, as required by the subject;
- inspect one owned account at a time;
- list all accounts owned by the logged-in user;
- reject access to another user's account;
- remove an existing owned account;
- transfer ownership to another registered user.

### Transactions

- deposit into `current` and `savings` accounts;
- withdraw from `current` and `savings` accounts;
- reject non-positive transaction amounts;
- reject withdrawals above the available balance;
- reject all transactions for `fixed01`, `fixed02`, and `fixed03` accounts;
- persist the new balance immediately.

## 💰 Interest rules

The implementation follows the values required by the official audit.

| Account type | Rule | Audit example for `$1001.20` created `10/10/2012` |
| --- | --- | --- |
| `current` | no interest | no interest message |
| `savings` | 7% annual rate, paid monthly | `$5.84` every month on day `10` |
| `fixed01` | 4% × 1 year | `$40.05` on `10/10/2013` |
| `fixed02` | 5% × 2 years | `$100.12` on `10/10/2014` |
| `fixed03` | 8% × 3 years | `$240.29` on `10/10/2015` |

`account.c` owns the account-type, date validation, maturity-date, and interest rules so the CLI and tests use the same calculations.

## 💾 Data storage

The mandatory implementation uses the assignment's text-file model:

```text
data/
├── users.txt
└── records.txt
```

`users.txt`:

```text
<id> <username> <password-hash>
```

Example:

```text
0 Alice sha256:d84464181f7f019f3fb10e6bbd06f543d7ac84c4f8e360ebb9402a472ab30ebc
```

`records.txt`:

```text
<id> <user_id> <owner> <account_number> <dd/mm/yyyy> <country> <phone> <balance> <type>
```

Writes use a temporary file followed by `rename`, so an account update does not rewrite the live storage in place.

Tests set `ATM_DATA_DIR` to a temporary directory, which keeps the repository's sample data unchanged.

## 🔔 Transfer notifications

On POSIX systems, logging in creates a per-user FIFO under `/tmp` and a small child listener process. If another active terminal transfers an account to that user, the receiver immediately sees a message similar to:

```text
[NOTIFICATION] You received account 777 from Alice.
```

This implements the assignment's instant-notification bonus without changing the mandatory storage format.

The transfer itself works on all supported builds. Native Windows disables only the FIFO notification bonus; use WSL/Linux to test that audit item.

## 🧪 Tests and audit

Run all available checks:

```bash
make check
```

This performs a clean build with:

```text
-std=c11 -Wall -Wextra -Werror -pedantic
```

and then runs:

- unit checks for date parsing and every official interest value;
- a full scripted audit flow covering registration, duplicate users, login, create/update/check, all four interest examples, fixed-account restrictions, overdraft rejection, deposit/withdraw, remove, missing-account errors, and ownership transfer;
- a two-process POSIX test proving that the receiving user gets the transfer notification immediately.

Run AddressSanitizer and UndefinedBehaviorSanitizer against the main audit flow:

```bash
make sanitize
```

The main executable audit scenario is in:

```text
tests/audit_flow.sh
```

The cross-terminal bonus scenario is in:

```text
tests/notification_flow.sh
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
│   └── utils.c
├── tests/
│   ├── fixtures/
│   ├── audit_flow.sh
│   ├── notification_flow.sh
│   └── test_interest.c
├── AGENTS.md
├── Makefile
├── README.md
└── README_RU.md
```

The repository is intentionally small: shared contracts are in `header.h`, business rules are isolated from interactive input, and persistence is centralized in `storage.c`.

## ⚠️ Notes

- The required backend is the original text-file storage. SQLite is not used, so the optional relational-database bonus is not claimed.
- SHA-256 is included for the educational password-protection bonus; a production authentication system should use a slow salted password KDF such as Argon2, scrypt, or bcrypt instead.
- The FIFO notification bonus is POSIX-specific. The rest of the ATM functionality does not depend on it.
- No external C libraries are required.

## 🧑‍💻 Author

- Nazar Yestayev (@nyestaye)
