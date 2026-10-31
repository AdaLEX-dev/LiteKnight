#!/usr/bin/env bash
# Run tests
# Usage: ./tools/test.sh [Debug|RelWithDebInfo|Release] [category]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

CONFIG="${1:-Debug}"
CATEGORY="${2:-all}"

echo "=== Soul Knight Linux Tests ==="
echo "Config: ${CONFIG}"
echo "Category: ${CATEGORY}"
echo

# Verify binary exists
TEST_BINARY="${BUILD_DIR}/soul-knight-test"
if [ ! -f "${TEST_BINARY}" ]; then
    echo "Test binary not found: ${TEST_BINARY}"
    echo "Build tests first: ./tools/build.sh ${CONFIG} soul-knight-test"
    exit 1
fi

# Run tests based on category
case "${CATEGORY}" in
    all)
        FILTER=""
        ;;
    unit)
        FILTER="--test-suite=unit"
        ;;
    integration|gameplay|regression|serialization|procedural|performance)
        echo "No ${CATEGORY} tests exist yet."
        echo "Add the tests before adding a CTest category; do not run an empty filter."
        exit 1
        ;;
    *)
        echo "Unknown category: ${CATEGORY}"
        echo "Categories: all, unit, integration, gameplay, regression, serialization, procedural, performance"
        exit 1
        ;;
esac

# Run
cd "${PROJECT_ROOT}"
echo "Running tests: ${CATEGORY}"
echo "---"

if [ -n "${FILTER}" ]; then
    "${TEST_BINARY}" ${FILTER} --reporter=console --success
else
    "${TEST_BINARY}" --reporter=console --success
fi

TEST_RESULT=$?

if [ ${TEST_RESULT} -eq 0 ]; then
    echo
    echo "=== All Tests Passed ==="
else
    echo
    echo "=== Tests Failed ==="
    exit ${TEST_RESULT}
fi