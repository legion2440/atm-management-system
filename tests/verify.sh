#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

printf 'ATM verification suite\n'
printf '======================\n\n'

"$ROOT/tests/bin/test_interest"
printf '[PASS] Interest and date unit tests\n\n'

bash "$ROOT/tests/core_flow.sh"
printf '\n'

bash "$ROOT/tests/bonus_flow.sh"
printf '\n'

bash "$ROOT/tests/notification_flow.sh"
printf '\nAll verification groups completed successfully.\n'
