#!/usr/bin/env bash
# Metadata asset audit; safe to run in CI without proprietary reference assets.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
exec python3 "${ROOT}/tools/asset/audit_assets.py" "$@"
