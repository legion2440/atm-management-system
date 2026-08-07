#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/data"
cp "$ROOT/tests/fixtures/data/users.txt" "$TMP/data/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$TMP/data/records.txt"

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 "$ROOT/atm" >"$TMP/setup.out" <<'INPUT'
2
Alice
1234password
8
1
SaltOne
shared-password
1
SaltTwo
shared-password
3
INPUT

DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import re
import sqlite3

pattern = re.compile(r"pbkdf2-sha256\$100000\$[0-9a-f]{32}\$[0-9a-f]{64}")
con = sqlite3.connect(os.environ["DB_PATH"])
rows = dict(con.execute("SELECT name,password FROM users WHERE name IN ('Alice','SaltOne','SaltTwo')"))
assert set(rows) == {"Alice", "SaltOne", "SaltTwo"}
assert all(pattern.fullmatch(value) for value in rows.values())
assert rows["SaltOne"] != rows["SaltTwo"]
PY
printf '[PASS] Legacy credentials migrate to PBKDF2\n'
printf '[PASS] Equal passwords receive different salts\n'

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 ATM_LOGIN_LOCK_SECONDS=3600 "$ROOT/atm" >"$TMP/failures.out" <<'INPUT'
2
Alice
wrong-1
2
Alice
wrong-2
2
Alice
wrong-3
2
Alice
wrong-4
2
Alice
wrong-5
3
INPUT

grep -Fq 'Too many failed login attempts. Try again later.' "$TMP/failures.out"
DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import sqlite3
import time

con = sqlite3.connect(os.environ["DB_PATH"])
row = con.execute("SELECT failed_attempts,locked_until FROM login_security WHERE name='Alice'").fetchone()
assert row is not None
assert row[0] == 5
assert row[1] > int(time.time())
PY
printf '[PASS] Five failed attempts create a persisted lockout\n'

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 ATM_LOGIN_LOCK_SECONDS=3600 "$ROOT/atm" >"$TMP/locked.out" <<'INPUT'
2
Alice
1234password
3
INPUT

grep -Fq 'Too many failed login attempts. Try again later.' "$TMP/locked.out"
if grep -Fq 'Welcome, Alice.' "$TMP/locked.out"; then
    printf '[FAIL] Locked account accepted the correct password\n'
    exit 1
fi
printf '[PASS] Lockout survives a process restart\n'

DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import sqlite3
con = sqlite3.connect(os.environ["DB_PATH"])
con.execute("DELETE FROM login_security WHERE name='Alice'")
con.commit()
PY

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 "$ROOT/atm" >"$TMP/recovered.out" <<'INPUT'
2
Alice
1234password
8
3
INPUT
grep -Fq 'Welcome, Alice.' "$TMP/recovered.out"
printf '[PASS] Login succeeds after lock state is cleared\n'

printf '\n5/5 security cases passed\n'
