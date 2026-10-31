#!/usr/bin/env bash
# Reproducible UnityPy wrapper.
#
# Creates a local tool venv on first use, installs the pinned UnityPy version,
# and runs the extraction tool. Extracted assets go to assets/generated/ and are
# gitignored; only analysis/assets/extraction_manifest.json is committed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VENV="${UNITYPY_VENV:-${SCRIPT_DIR}/.venv}"

if [[ ! -x "${VENV}/bin/python" ]]; then
    echo "Creating UnityPy venv at ${VENV}" >&2
    python3 -m venv "${VENV}"
    "${VENV}/bin/python" -m pip install --quiet --upgrade pip
    "${VENV}/bin/python" -m pip install --quiet -r "${SCRIPT_DIR}/requirements.txt"
fi

exec "${VENV}/bin/python" "${SCRIPT_DIR}/extract_unity.py" "$@"
