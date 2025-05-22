#!/bin/bash
# Script to build and run tests for the UDP Gateway component

set -e  # Exit on any error

echo "UDP Gateway Test Runner"
echo "======================"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: This script should be run from the UDP Gateway component directory"
    echo "Current directory: $(pwd)"
    echo "Expected files: CMakeLists.txt, src/, include/, test/"
    exit 1
fi

# Create build directory for tests
BUILD_DIR="build-test"
echo "Creating build directory: $BUILD_DIR"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with testing enabled
echo "Configuring CMake with testing enabled..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCMAKE_CXX_FLAGS="-w"

# Build the tests
echo "Building tests..."
cmake --build . -j$(nproc)

# Check if tests were built successfully
if [ ! -d "test" ]; then
    echo "Error: Test directory not found. Tests may not have been built properly."
    exit 1
fi

# Run the tests
echo "Running tests..."
echo "================"

# Run with verbose output
ctest --verbose

# Also run individual tests for more detailed output
echo ""
echo "Running individual tests for detailed output:"
echo "=============================================="

if [ -f "test/types_test" ]; then
    echo ""
    echo "Types Test:"
    echo "-----------"
    ./test/types_test
fi

if [ -f "test/rate_limiter_test" ]; then
    echo ""
    echo "Rate Limiter Test:"
    echo "------------------"
    ./test/rate_limiter_test
fi

if [ -f "test/session_test" ]; then
    echo ""
    echo "Session Test:"
    echo "-------------"
    ./test/session_test
fi

if [ -f "test/gateway_test" ]; then
    echo ""
    echo "Gateway Test:"
    echo "-------------"
    ./test/gateway_test
fi

echo ""
echo "All tests completed!"
echo "===================="

# Go back to original directory
cd ..