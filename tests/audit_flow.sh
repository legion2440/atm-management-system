#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/data"
cp "$ROOT/tests/fixtures/data/users.txt" "$TMP/data/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$TMP/data/records.txt"

ATM_DATA_DIR="$TMP/data" ATM_NO_COLOR=1 "$ROOT/atm" >"$TMP/output.txt" <<'INPUT'
1
Marcus
q1w2e3r4t5y6
1
Alice
anything
2
Alice
1234password
1
834213
10/10/2012
UK
291231392
1001.20
saving
1
320421
10/10/2012
UK
291231392
1001.20
fixed01
1
3214
10/10/2012
UK
291231392
1001.20
fixed02
1
3212
10/10/2012
UK
291231392
1001.20
fixed03
2
834213
1
777777777
2
834213
2
KZ
3
834213
3
320421
3
3214
3
3212
5
3212
5
834213
2
5000
5
834213
2
1.20
5
834213
1
1.20
6
834213
6
320421
6
3214
6
999999
7
3212
Michel
8
2
Michel
password1234
4
8
3
INPUT

grep -Fq "Storage: SQLite" "$TMP/output.txt"
grep -Fq "User 'Marcus' registered successfully." "$TMP/output.txt"
grep -Fq "User 'Alice' already exists." "$TMP/output.txt"
grep -Fq "Account information updated successfully." "$TMP/output.txt"
grep -Fq 'You will get $5.84 as interest on day 10 of every month' "$TMP/output.txt"
grep -Fq 'You will get $40.05 as interest on 10/10/2013' "$TMP/output.txt"
grep -Fq 'You will get $100.12 as interest on 10/10/2014' "$TMP/output.txt"
grep -Fq 'You will get $240.29 as interest on 10/10/2015' "$TMP/output.txt"
grep -Fq 'Transactions are not allowed for fixed accounts.' "$TMP/output.txt"
grep -Fq 'Withdrawal denied: amount exceeds the available balance.' "$TMP/output.txt"
grep -Fq 'Transaction completed. New balance: $1000.00' "$TMP/output.txt"
grep -Fq 'Transaction completed. New balance: $1001.20' "$TMP/output.txt"
grep -Fq 'Account does not exist for this user.' "$TMP/output.txt"
grep -Fq 'Account 3212 transferred to Michel successfully.' "$TMP/output.txt"
grep -Fq 'Owner: Michel' "$TMP/output.txt"

DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import re
import sqlite3

con = sqlite3.connect(os.environ["DB_PATH"])
users = con.execute("SELECT id, name, password FROM users ORDER BY id").fetchall()
assert len([row for row in users if row[1] == "Alice"]) == 1
marcus = next(row for row in users if row[1] == "Marcus")
assert re.fullmatch(r"sha256:[0-9a-f]{64}", marcus[2])
assert "q1w2e3r4t5y6" not in marcus[2]
row = con.execute(
    "SELECT u.name, a.account_number, a.country, a.phone, a.balance, a.type "
    "FROM accounts a JOIN users u ON u.id=a.user_id WHERE a.account_number=3212"
).fetchone()
assert row == ("Michel", 3212, "UK", "291231392", 1001.2, "fixed03")
PY

printf 'audit flow: OK\n'
