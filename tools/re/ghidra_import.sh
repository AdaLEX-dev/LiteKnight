#!/usr/bin/env bash
# Import libil2cpp.so into Ghidra and apply Il2CppDumper method names.
#
# This is the optional Ghidra path. The primary pipeline is llvm-objdump via
# tools/re/il2cpp_re.py; Ghidra is useful for interactive decompilation.
#
# Usage:
#   tools/re/ghidra_import.sh [project-dir] [project-name]
#
# Environment:
#   GHIDRA_HOME  default /home/jules/bin/ghidra_12.2_PUBLIC
#   SOULKNIGHT_BINARY optional override

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
GHIDRA_HOME="${GHIDRA_HOME:-/home/jules/bin/ghidra_12.2_PUBLIC}"
BINARY="${SOULKNIGHT_BINARY:-${PROJECT_ROOT}/analysis/apktool_out/lib/arm64-v8a/libil2cpp.so}"
PROJECT_DIR="${1:-${PROJECT_ROOT}/analysis/re/ghidra_project}"
PROJECT_NAME="${2:-SoulKnight268}"
SYMBOLS="${PROJECT_ROOT}/analysis/re/generated/il2cpp_symbols.tsv"
HEADLESS="${GHIDRA_HOME}/support/analyzeHeadless"

if [[ ! -x "${HEADLESS}" ]]; then
    echo "Ghidra headless not found: ${HEADLESS}" >&2
    echo "Set GHIDRA_HOME to a Ghidra installation." >&2
    exit 1
fi
if [[ ! -f "${BINARY}" ]]; then
    echo "libil2cpp.so not found: ${BINARY}" >&2
    exit 1
fi

mkdir -p "${PROJECT_DIR}" "${PROJECT_ROOT}/analysis/re/generated"

"${PROJECT_ROOT}/tools/re/il2cpp_re.py" export-symbols --out "${SYMBOLS}"

echo "Importing ${BINARY} with symbols from ${SYMBOLS}"
echo "Project: ${PROJECT_DIR}/${PROJECT_NAME}"

# -noanalysis keeps the first import fast. The post-script creates labels and
# functions as it walks the symbol table. Run a normal Ghidra analysis later
# for full cross-reference/decompiler quality.
"${HEADLESS}" "${PROJECT_DIR}" "${PROJECT_NAME}" \
    -import "${BINARY}" \
    -overwrite \
    -noanalysis \
    -scriptPath "${SCRIPT_DIR}/ghidra" \
    -postScript ApplyIl2CppSymbols.java "${SYMBOLS}"

echo "Done. Open with: ${GHIDRA_HOME}/ghidraRun ${PROJECT_DIR}/${PROJECT_NAME}.gpr"
