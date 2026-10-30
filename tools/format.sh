#!/usr/bin/env bash
# Format code with clang-format
# Usage: ./tools/format.sh [check|fix]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

MODE="${1:-fix}"

if ! command -v clang-format &> /dev/null; then
    echo "clang-format not found. Install: sudo pacman -S clang"
    exit 1
fi

# Find all C++ files
FILES=()
while IFS= read -r -d '' file; do
    FILES+=("$file")
done < <(find "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/tests" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) -print0 2>/dev/null)

if [ ${#FILES[@]} -eq 0 ]; then
    echo "No C++ files found"
    exit 0
fi

echo "=== Formatting ${#FILES[@]} files ==="

if [ "${MODE}" = "check" ]; then
    clang-format --dry-run --Werror "${FILES[@]}"
    RESULT=$?
    if [ ${RESULT} -eq 0 ]; then
        echo "All files properly formatted"
    else
        echo "Formatting issues found. Run './tools/format.sh fix' to fix."
    fi
    exit ${RESULT}
else
    clang-format -i "${FILES[@]}"
    echo "Formatting complete"
fi