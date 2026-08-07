#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cp "$ROOT/tests/fixtures/data/users.txt" "$ROOT/data/users.txt"
cp "$ROOT/tests/fixtures/data/records.txt" "$ROOT/data/records.txt"
echo "Sample data restored."
