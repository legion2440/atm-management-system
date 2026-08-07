#!/usr/bin/env bash
set -euo pipefail

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "[SKIP] Instant transfer notification (POSIX FIFO is disabled on native Windows)"
        exit 0
        ;;
esac

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=$(mktemp -d)
MICHEL_PID=''
cleanup() {
    if [[ -n "$MICHEL_PID" ]]; then
        kill "$MICHEL_PID" 2>/dev/null || true
    fi
    rm -f /tmp/atm-management-Alice.fifo /tmp/atm-management-Michel.fifo
    rm -rf "$TMP"
}
trap cleanup EXIT

mkdir -p "$TMP/data"
cp "$ROOT/tests/fixtures/data/users.txt" "$TMP/data/users.txt"
cat >"$TMP/data/records.txt" <<'DATA'
0 0 Alice 777 10/10/2012 UK 291231392 100.00 current
DATA
mkfifo "$TMP/michel.in"

ATM_DATA_DIR="$TMP/data" "$ROOT/atm" <"$TMP/michel.in" >"$TMP/michel.out" 2>&1 &
MICHEL_PID=$!
exec 3>"$TMP/michel.in"
printf '2\nMichel\npassword1234\n' >&3

for _ in $(seq 1 100); do
    [[ -p /tmp/atm-management-Michel.fifo ]] && break
    sleep 0.02
done
[[ -p /tmp/atm-management-Michel.fifo ]]

ATM_DATA_DIR="$TMP/data" "$ROOT/atm" >"$TMP/alice.out" <<'INPUT'
2
Alice
1234password
7
777
Michel
10
3
INPUT

for _ in $(seq 1 100); do
    if grep -Fq '[NOTIFICATION] You received account 777 from Alice.' "$TMP/michel.out" 2>/dev/null; then
        break
    fi
    sleep 0.02
done

grep -Fq '[NOTIFICATION] You received account 777 from Alice.' "$TMP/michel.out"
printf '10\n3\n' >&3
exec 3>&-
wait "$MICHEL_PID"
MICHEL_PID=''

printf '[PASS] Instant transfer notification\n'
