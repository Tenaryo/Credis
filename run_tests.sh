#!/bin/bash

set -e

BUILD_DIR="build"
TEST_DIR="tests"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Running Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake .. -DCMAKE_BUILD_TYPE=Debug
fi

echo -e "${YELLOW}Building tests...${NC}"
make 2>&1 | grep -E "(Building|Linking|error|warning)" || true
echo

TEST_EXECUTABLES=()
for test_file in ../$TEST_DIR/*.cpp; do
    test_name=$(basename "$test_file" .cpp)
    test_executable="$TEST_DIR/$test_name"
    if [ -f "$test_executable" ]; then
        TEST_EXECUTABLES+=("$test_executable|$test_name")
    fi
done

if [ ${#TEST_EXECUTABLES[@]} -eq 0 ]; then
    echo -e "${YELLOW}No test executables found${NC}"
    exit 0
fi

TOTAL_TESTS=${#TEST_EXECUTABLES[@]}
PASSED=0
FAILED=0
FAILED_TESTS=()

for test_entry in "${TEST_EXECUTABLES[@]}"; do
    IFS='|' read -r test_executable test_name <<< "$test_entry"
    
    echo -e "${BLUE}----------------------------------------${NC}"
    echo -e "${BLUE}Running: $test_name${NC}"
    echo -e "${BLUE}----------------------------------------${NC}"
    
    if ./"$test_executable"; then
        PASSED=$((PASSED + 1))
        echo -e "${GREEN}✓ $test_name PASSED${NC}"
    else
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_name")
        echo -e "${RED}✗ $test_name FAILED${NC}"
    fi
    
    echo
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total:  $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo

if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed tests:${NC}"
    for test_name in "${FAILED_TESTS[@]}"; do
        echo -e "  ${RED}✗ $test_name${NC}"
    done
    echo
    exit 1
fi

echo -e "${GREEN}All tests passed! ✓${NC}"
exit 0
