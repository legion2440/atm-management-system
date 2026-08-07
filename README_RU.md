# ATM Management System

Терминальная ATM-система на C. Реализован весь обязательный scope 01-edu и все пункты официального bonus-раздела: relational database, улучшенный терминальный интерфейс, защищённое хранение паролей, мгновенные уведомления, собственный Makefile, дополнительные функции и рефакторинг/оптимизация starter-кода.

Основное runtime-хранилище — SQLite. Исходные текстовые файлы задания сохранены как seed и совместимый fallback.

· [English version](README.md)  
· [School repository](https://01.tomorrow-school.ai/git/nyestaye/atm-management-system)  
· [Задание 01-edu](https://github.com/01-edu/public/tree/master/subjects/atm-management-system)

## 📋 Оглавление

- [🚀 Быстрый запуск](#-быстрый-запуск)
- [📝 О проекте](#-о-проекте)
- [✨ Возможности](#-возможности)
- [🔐 Защита](#-защита)
- [🎁 Бонусы](#-бонусы)
- [💰 Проценты](#-проценты)
- [💾 Хранилище](#-хранилище)
- [🔔 Уведомления](#-уведомления)
- [🧪 Тесты и проверка](#-тесты-и-проверка)
- [📁 Структура](#-структура)
- [⚠️ Примечания](#️-примечания)
- [🧑‍💻 Автор](#-автор)

## 🚀 Быстрый запуск

### Требования

- GCC или Clang с C11
- GNU Make
- SQLite development library
- Bash и Python 3 для автоматических проверок
- Linux / WSL для FIFO-бонуса и multi-process concurrency checks

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev python3
```

Сборка и запуск:

```bash
git clone https://01.tomorrow-school.ai/git/nyestaye/atm-management-system
cd atm-management-system
make
./atm
```

Seed-пользователи:

| Пользователь | Пароль |
| --- | --- |
| `Alice` | `1234password` |
| `Michel` | `password1234` |

При первом запуске создаётся `data/atm.db`, данные импортируются из seed-файлов. Старые seed-credentials читаются для совместимости и после успешного login автоматически мигрируют на salted PBKDF2.

Сбросить sample data и состояние login security:

```bash
bash scripts/reset_data.sh
```

## 📝 О проекте

Обязательные пункты меню оставлены на номерах 1–8, чтобы официальный сценарий проверки выполнялся без специальных режимов:

1. Создать счёт
2. Обновить данные счёта
3. Проверить счёт и проценты
4. Показать свои счета
5. Выполнить транзакцию
6. Удалить счёт
7. Передать владельца
8. Logout
9. Сменить пароль `[bonus]`
10. Сводка по счетам `[bonus]`

Код разделён на отдельные модули account rules, auth, persistence, input, notification, UI и session actions.

## ✨ Возможности

- регистрация и запрет одинаковых username;
- login по сохранённым данным;
- salted `PBKDF2-HMAC-SHA256` password storage;
- автоматическая миграция старых `sha256:` / plaintext seed credentials после успешного login;
- persistent lockout после повторных неверных попыток входа;
- смена пароля с проверкой текущего;
- `current`, `saving/savings`, `fixed01`, `fixed02`, `fixed03`;
- создание, изменение, просмотр, список и удаление своих счетов;
- deposit / withdraw для `current` и `savings`;
- запрет overdraft и транзакций по fixed-счетам;
- transfer ownership другому пользователю;
- сводка: количество счетов, total balance и разбивка по типам.

## 🔐 Защита

Дополнительно к функциональным требованиям сделано следующее:

- **Password KDF:** новые пароли сохраняются через `PBKDF2-HMAC-SHA256`, 100 000 итераций и отдельный 128-bit salt. Одинаковые пароли дают разные stored values.
- **Credential migration:** старые `sha256:` и plaintext seed-записи остаются только для совместимости; после успешного входа они переписываются в PBKDF2-формат.
- **Brute-force protection:** после 5 неверных попыток для одного username включается блокировка на 30 секунд. Состояние хранится в SQLite, поэтому перезапуск программы блокировку не сбрасывает. `ATM_LOGIN_LOCK_SECONDS` используется для deterministic tests и controlled deployments.
- **Authorization:** операции со счётом проверяют и `user_id`, и номер счёта, поэтому знания одного account number недостаточно для доступа к чужому счёту.
- **SQL injection resistance:** пользовательские данные передаются в SQLite через prepared statements и bound parameters.
- **Concurrent money safety:** deposit/withdraw выполняются внутри SQLite write transaction. Multi-process test проверяет отсутствие lost updates и невозможность concurrent overdraft.

## 🎁 Бонусы

| Bonus | Статус | Реализация |
| --- | --- | --- |
| Мгновенное уведомление о transfer | ✅ | POSIX FIFO + child listener |
| Обновлённый terminal interface | ✅ | TTY-aware ANSI colors, рамки, section headers |
| Защищённые пароли | ✅ | salted PBKDF2 + migration + lockout |
| Relational database | ✅ | SQLite `users` + `accounts`, FK, constraints, index |
| Собственный Makefile | ✅ | build/verify/sanitize/text-only targets |
| Дополнительные функции | ✅ | смена пароля + account summary |
| Оптимизация исходного кода | ✅ | модульный рефакторинг, prepared statements, targeted writes, transactions, indexes, общий validation/rules layer |

Полная карта evidence: [`TEST_EVIDENCE.md`](TEST_EVIDENCE.md).

## 💰 Проценты

Расчёты совпадают с контрольными значениями официального checklist.

| Тип | Правило | Для `$1001.20`, дата `10/10/2012` |
| --- | --- | --- |
| `current` | без процентов | сообщение об отсутствии процентов |
| `savings` | 7% годовых / 12 | `$5.84` каждого месяца 10 числа |
| `fixed01` | 4% × 1 год | `$40.05` на `10/10/2013` |
| `fixed02` | 5% × 2 года | `$100.12` на `10/10/2014` |
| `fixed03` | 8% × 3 года | `$240.29` на `10/10/2015` |

## 💾 Хранилище

По умолчанию используется SQLite:

```text
data/atm.db
```

Схема:

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

Есть индекс `idx_accounts_user_id`. User/account writes используют prepared statements. Изменения balance идут в явных write transactions; create/update/delete/transfer выполняются targeted SQL-операциями, а не полной перезаписью таблицы.

Исходные файлы задания используются как seed:

```text
data/users.txt
data/records.txt
```

Text fallback:

```bash
ATM_STORAGE=text ./atm
```

Сборка без SQLite:

```bash
make fclean
make TEXT_ONLY=1
```

Text backend сохраняет atomic file replacement и persistent login lockout, но остаётся compatibility-mode и не предназначен для нескольких concurrent writers. Multi-process balance guarantees относятся к default SQLite backend.

## 🔔 Уведомления

На POSIX при login создаётся FIFO пользователя в `/tmp` и отдельный listener-process. Если во втором терминале этому пользователю передать счёт, уведомление появляется сразу:

```text
[NOTIFICATION] You received account 777 from Alice.
```

На native Windows transfer работает, но FIFO-бонус отключён. Для этого пункта используйте WSL/Linux.

## 🧪 Тесты и проверка

Для проверяющего достаточно одной команды:

```bash
make verify
```

Набор проверок включает:

- unit boundary checks для дат, високосных годов, maturity dates и процентов;
- 25 core-кейсов: регистрация, login, создание/обновление счетов, проценты, транзакции, удаление, transfer ownership и persistence;
- 20 edge cases: неверные credentials/input, duplicate/negative account number, invalid date/type, negative balance, zero/negative transaction, invalid action, missing/self transfer, доступ прежнего владельца, ошибки смены пароля, whitespace, oversized input и целостность persistent state;
- 5 security cases: PBKDF2 migration, разные salts, persisted lockout и восстановление login после очистки lock state;
- 2 multi-process concurrency cases: отсутствие lost deposits и невозможность concurrent overdraft в SQLite;
- дополнительные проверки SQLite schema/FK/index, password storage, TUI, account summary, text fallback и instant notification между активными сессиями.

Чистая пересборка плюс тот же набор:

```bash
make check
```

ASan + UBSan:

```bash
make sanitize
```

CI дополнительно собирает text-only вариант:

```bash
make TEXT_ONLY=1
```

## 📁 Структура

```text
atm-management-system/
├── .github/workflows/ci.yml
├── agent/module-index.md
├── data/
│   ├── records.txt
│   └── users.txt
├── scripts/reset_data.sh
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

## ⚠️ Примечания

- `atm.db` — основное runtime-хранилище; `users.txt` и `records.txt` являются seed/fallback.
- PBKDF2 реализован внутри проекта, чтобы не добавлять тяжёлую внешнюю dependency. В production обычно предпочитают memory-hard KDF вроде Argon2id и подбирают cost под железо.
- Lockout здесь намеренно простой: 5 ошибок и 30 секунд. В реальной банковской системе дополнительно нужны rate limiting, monitoring, device/session controls и operational security.
- Runtime SQLite и text-mode login-security files находятся в `.gitignore` и пересоздаются после `scripts/reset_data.sh`.
- FIFO notification — POSIX-specific, остальная логика от него не зависит.

## 🧑‍💻 Автор

- Nazar Yestayev (@nyestaye)
