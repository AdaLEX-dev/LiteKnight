#!/usr/bin/env bash
# Audit committed metadata and, when available, local generated pixels.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${ROOT}/tools/asset/audit_assets.py" --require-generated "$@"
