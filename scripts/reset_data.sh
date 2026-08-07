#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cp "$ROOT/tests/fixtures/data/users.txt" "$ROOT/data/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$ROOT/data/records.txt"
rm -f "$ROOT/data/atm.db" "$ROOT/data/atm.db-shm" "$ROOT/data/atm.db-wal" "$ROOT/data/login_security.txt"
echo "Sample data restored. Runtime storage and login security state will be recreated on next start."
