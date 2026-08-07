#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/data"
cp "$ROOT/tests/fixtures/data/users.txt" "$TMP/data/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$TMP/data/records.txt"

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 "$ROOT/atm" >"$TMP/output.txt" <<'INPUT'
2
Alice
1234password
9
8
1234password
newpass123
newpass123
10
2
Alice
newpass123
9
10
3
INPUT

grep -Fq '+==========================================+' "$TMP/output.txt"
grep -Fq 'Storage: SQLite' "$TMP/output.txt"
printf '[PASS] Framed terminal interface\n'

grep -Fq 'Accounts: 0' "$TMP/output.txt"
printf '[PASS] Account summary\n'

grep -Fq 'Password changed successfully.' "$TMP/output.txt"
[[ $(grep -Fc 'Welcome, Alice.' "$TMP/output.txt") -eq 2 ]]
printf '[PASS] Authenticated password change\n'

DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import re
import sqlite3

con = sqlite3.connect(os.environ["DB_PATH"])
tables = {row[0] for row in con.execute("SELECT name FROM sqlite_master WHERE type='table'")}
assert {"users", "accounts", "login_security"}.issubset(tables)
indexes = {row[1] for row in con.execute("PRAGMA index_list(accounts)")}
assert "idx_accounts_user_id" in indexes
foreign_keys = con.execute("PRAGMA foreign_key_list(accounts)").fetchall()
assert any(row[2] == "users" and row[3] == "user_id" for row in foreign_keys)
password = con.execute("SELECT password FROM users WHERE name='Alice'").fetchone()[0]
assert re.fullmatch(r"pbkdf2-sha256\$100000\$[0-9a-f]{32}\$[0-9a-f]{64}", password)
assert password != "newpass123"
PY
printf '[PASS] Relational SQLite schema and index\n'
printf '[PASS] Salted password storage\n'

mkdir -p "$TMP/text"
cp "$ROOT/tests/fixtures/data/users.txt" "$TMP/text/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$TMP/text/records.txt"
ATM_STORAGE=text ATM_DATA_DIR="$TMP/text" ATM_NO_COLOR=1 "$ROOT/atm" >"$TMP/text.out" <<'INPUT'
3
INPUT
grep -Fq 'Storage: text files' "$TMP/text.out"
[[ ! -e "$TMP/text/atm.db" ]]
printf '[PASS] Text storage fallback\n'

printf '\n6/6 bonus cases passed\n'
