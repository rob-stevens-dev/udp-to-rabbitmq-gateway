#!/bin/bash
# Script to run a specific test for debugging

if [ $# -eq 0 ]; then
    echo "Usage: $0 <test_name>"
    echo "Available tests:"
    echo "  types_test"
    echo "  rate_limiter_test"
    echo "  session_test"
    echo "  gateway_test"
    exit 1
fi

TEST_NAME=$1

# Navigate to build-test directory
cd build-test 2>/dev/null || {
    echo "build-test directory not found. Running full build first..."
    ./run-tests.sh > /dev/null 2>&1
    cd build-test
}

if [ -f "test/${TEST_NAME}" ]; then
    echo "Running ${TEST_NAME}..."
    echo "========================"
    ./test/${TEST_NAME}
else
    echo "Test ${TEST_NAME} not found in test/ directory"
    echo "Available test files:"
    ls -1 test/
fi