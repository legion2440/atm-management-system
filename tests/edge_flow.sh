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
wrong-password
2
Alice
1234password
1
-1
1
1000
10/10/2012
KZ
123
-1
1
1000
10/10/2012
KZ
123
100
current
1
1000
1
2000
31/02/2012
10/10/2012
KZ
456
50
unknown
savings
1
3000
10/10/2012
KZ	BAD
1
3000
10/10/2012
KZ
12	34
5
1000
1
0
5
1000
2
-1
5
1000
9
7
1000
Nobody
7
1000
Alice
8
wrong-password
8
1234password

8
1234password
new-password
different-password
7
1000
Michel
3
1000
6
1000
10
2
Michel
password1234
3
1000
10
1

x
1
Bad	User
x
1
abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789TOOLONG
3
INPUT

passed=0
check_text() {
    local label=$1
    local expected=$2
    if ! grep -Fq "$expected" "$TMP/output.txt"; then
        printf '[FAIL] %s\n' "$label"
        printf 'Expected output: %s\n' "$expected"
        exit 1
    fi
    printf '[PASS] %s\n' "$label"
    passed=$((passed + 1))
}

check_count() {
    local label=$1
    local expected=$2
    local count=$3
    local actual
    actual=$(grep -Fc "$expected" "$TMP/output.txt" || true)
    if [[ "$actual" -ne "$count" ]]; then
        printf '[FAIL] %s\n' "$label"
        printf 'Expected %d occurrences, got %d: %s\n' "$count" "$actual" "$expected"
        exit 1
    fi
    printf '[PASS] %s\n' "$label"
    passed=$((passed + 1))
}

check_text "Reject wrong password" "Invalid username or password."
check_count "Reject negative and duplicate account numbers" "This account number already exists or is invalid." 2
check_text "Reject negative initial deposit" "Initial deposit cannot be negative."
check_text "Recover from invalid date" "Invalid date. Use dd/mm/yyyy."
check_text "Recover from invalid account type" "Invalid account type."
check_text "Reject whitespace in country" "Country must be a non-empty single token."
check_text "Reject whitespace in phone" "Phone number must be non-empty and contain no whitespace."
check_count "Reject zero and negative transaction amounts" "Transaction amount must be positive." 2
check_text "Reject invalid transaction choice" "Invalid choice."
check_text "Reject transfer to missing user" "Target user does not exist."
check_text "Reject transfer to current owner" "The account already belongs to this user."
check_text "Reject incorrect current password" "Current password is incorrect."
check_text "Reject empty replacement password" "New password cannot be empty."
check_text "Reject password confirmation mismatch" "New passwords do not match."
check_text "Allow valid ownership transfer" "Account 1000 transferred to Michel successfully."
check_count "Block former owner after transfer" "Account does not exist for this user." 2
check_text "New owner can inspect transferred account" "Owner: Michel"
check_count "Reject empty and whitespace usernames" "Username and password must be non-empty; usernames cannot contain whitespace." 2
check_text "Reject oversized input" "Input is too long."

DB_PATH="$TMP/data/atm.db" python3 - <<'PY'
import os
import sqlite3

con = sqlite3.connect(os.environ["DB_PATH"])
users = con.execute("SELECT name FROM users ORDER BY id").fetchall()
assert users == [("Alice",), ("Michel",)]
accounts = con.execute(
    "SELECT account_number, user_id, balance, type FROM accounts ORDER BY account_number"
).fetchall()
assert accounts == [
    (1000, 1, 100.0, "current"),
    (2000, 0, 50.0, "savings"),
]
PY
printf '[PASS] Rejected operations leave persistent state clean\n'
passed=$((passed + 1))

expected=20
if [[ "$passed" -ne "$expected" ]]; then
    printf '[FAIL] Edge case count mismatch: %d/%d\n' "$passed" "$expected"
    exit 1
fi
printf '\n%d/%d edge cases passed\n' "$passed" "$expected"
