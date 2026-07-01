#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="build"
COVERAGE=false
COVERAGE_DIR="coverage_report"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

usage() {
    echo "Usage: $0 [--coverage]"
    echo "  --coverage   Build with code coverage instrumentation and generate HTML report"
    exit 0
}

for arg in "$@"; do
    case "$arg" in
        --coverage) COVERAGE=true ;;
        --help|-h)  usage ;;
        *)          echo "Unknown option: $arg"; usage ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Running Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ -f CMakeCache.txt ]]; then
    EXTRA_ARGS="-DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTS=ON"
else
    EXTRA_ARGS="-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTS=ON"
fi

if $COVERAGE; then
    EXTRA_ARGS="$EXTRA_ARGS -DENABLE_COVERAGE=ON"
    echo -e "${BLUE}Coverage mode enabled${NC}"
    echo
fi

cmake .. -G Ninja $EXTRA_ARGS

echo -e "${YELLOW}Building tests...${NC}"
cmake --build . -j$(nproc)
echo

echo -e "${YELLOW}Running tests via ctest...${NC}"
echo

if ! ctest --output-on-failure -j$(nproc); then
    echo
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

echo
echo -e "${GREEN}All tests passed!${NC}"

if $COVERAGE; then
    echo
    echo -e "${YELLOW}Generating coverage report...${NC}"

    GCOV_TOOL=""
    if command -v gcov-14 &>/dev/null; then
        GCOV_TOOL="/usr/bin/gcov-14"
    elif command -v gcov-13 &>/dev/null; then
        GCOV_TOOL="/usr/bin/gcov-13"
    fi

    GCOV_ARG=""
    if [[ -n "$GCOV_TOOL" ]]; then
        GCOV_ARG="--gcov-tool $GCOV_TOOL"
    fi

    cd "$SCRIPT_DIR"

    lcov --capture --directory "$BUILD_DIR" --output-file "$BUILD_DIR/coverage.info" \
         $GCOV_ARG \
         --rc lcov_branch_coverage=1 \
         --base-directory . 2>/dev/null || true

    lcov --remove "$BUILD_DIR/coverage.info" '/usr/*' '*/tests/*' '*/_deps/*' '*/cmake_pch*' \
         --output-file "$BUILD_DIR/coverage_filtered.info" \
         $GCOV_ARG \
         --rc lcov_branch_coverage=1 2>/dev/null || true

    genhtml "$BUILD_DIR/coverage_filtered.info" --output-directory "$BUILD_DIR/$COVERAGE_DIR" \
            --title "Credis Coverage Report" \
            --rc lcov_branch_coverage=1 2>/dev/null || true

    SUMMARY=$(lcov --summary "$BUILD_DIR/coverage_filtered.info" --rc lcov_branch_coverage=1 2>&1)
    LINE_PCT=$(echo "$SUMMARY" | grep "lines" | sed 's/.*: \([0-9.]*\)%.*/\1/')
    LINE_INFO=$(echo "$SUMMARY" | grep "lines" | sed 's/.*(\(.*\)).*/\1/')
    FUNC_PCT=$(echo "$SUMMARY" | grep "functions" | sed 's/.*: \([0-9.]*\)%.*/\1/')
    FUNC_INFO=$(echo "$SUMMARY" | grep "functions" | sed 's/.*(\(.*\)).*/\1/')

    echo
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   Coverage Summary${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "  Lines:     ${LINE_PCT:-?}% (${LINE_INFO:-?})"
    echo -e "  Functions: ${FUNC_PCT:-?}% (${FUNC_INFO:-?})"
    echo
    echo -e "  HTML report: ${BUILD_DIR}/${COVERAGE_DIR}/index.html"
    echo
fi

exit 0
