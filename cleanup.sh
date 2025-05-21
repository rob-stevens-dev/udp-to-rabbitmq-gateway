#!/bin/bash
# Script to clean up our development environment and restore the repository to a clean state

echo "UDP-to-RabbitMQ Gateway Cleanup Script"
echo "====================================="
echo "This script will clean up temporary files and test directories created during development."
echo "Warning: This will remove all build artifacts and test files!"
echo ""
read -p "Are you sure you want to continue? (y/n): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    echo "Cleanup cancelled."
    exit 0
fi

# Define root directory (adjust if needed)
ROOT_DIR="$(pwd)"
echo "Using root directory: $ROOT_DIR"
echo ""

echo "Step 1: Removing build directories..."
rm -rf "$ROOT_DIR/build"
rm -rf "$ROOT_DIR/minimal_build"
rm -rf "$ROOT_DIR/components/udp-gateway/build"
echo "✓ Build directories removed"

echo "Step 2: Removing mock interfaces..."
rm -rf "$ROOT_DIR/mock_includes"
rm -rf "$ROOT_DIR/components/udp-gateway/include/mock_interfaces"
echo "✓ Mock interfaces removed"

echo "Step 3: Removing temporary component interface directories..."
rm -rf "$ROOT_DIR/components/protocol-parser"
rm -rf "$ROOT_DIR/components/redis-deduplication"
rm -rf "$ROOT_DIR/components/rabbitmq-integration"
rm -rf "$ROOT_DIR/components/device-manager"
rm -rf "$ROOT_DIR/components/monitoring-system"
echo "✓ Temporary component directories removed"

echo "Step 4: Removing test files..."
rm -f "$ROOT_DIR/test_minimal.cpp"
echo "✓ Test files removed"

echo "Step 5: Cleaning up Docker artifacts (if Docker is installed)..."
if command -v docker &> /dev/null; then
    # Stop and remove any running containers related to our project
    docker ps -a | grep "udp-gateway\|redis\|rabbitmq" | awk '{print $1}' | xargs -r docker stop
    docker ps -a | grep "udp-gateway\|redis\|rabbitmq" | awk '{print $1}' | xargs -r docker rm
    
    # Remove any related images
    docker images | grep "udp-gateway" | awk '{print $3}' | xargs -r docker rmi
    
    # Remove any related volumes
    docker volume ls | grep "udp-to-rabbitmq-gateway" | awk '{print $2}' | xargs -r docker volume rm
    
    echo "✓ Docker artifacts cleaned"
else
    echo "× Docker not found - skipping Docker cleanup"
fi

echo "Step 6: Checking for other project-related artifacts..."
# Find any remaining files that might be related to our test builds
EXTRA_FILES=$(find "$ROOT_DIR" -name "*udp-gateway*" -not -path "*/components/udp-gateway/*" -not -path "*/\.*" 2>/dev/null)
if [ -n "$EXTRA_FILES" ]; then
    echo "The following files might also be related to our test builds:"
    echo "$EXTRA_FILES"
    echo ""
    read -p "Do you want to remove these files too? (y/n): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "$EXTRA_FILES" | xargs rm -rf
        echo "✓ Additional files removed"
    else
        echo "× Keeping additional files"
    fi
else
    echo "✓ No additional project-related artifacts found"
fi

echo "Step 7: Restoring original component structure..."
# Ensure the components/udp-gateway directory has the expected structure
mkdir -p "$ROOT_DIR/components/udp-gateway/include/udp_gateway"
mkdir -p "$ROOT_DIR/components/udp-gateway/src"
mkdir -p "$ROOT_DIR/components/udp-gateway/test/unit"
mkdir -p "$ROOT_DIR/components/udp-gateway/test/integration"
mkdir -p "$ROOT_DIR/components/udp-gateway/examples"
mkdir -p "$ROOT_DIR/components/udp-gateway/cmake"
echo "✓ Component structure restored"

echo ""
echo "Cleanup Complete!"
echo "The repository has been restored to a clean state."
echo "Original source code files in components/udp-gateway have been preserved."
echo ""
echo "To fully reset the repository, you may want to run: git clean -fdx"
echo "Note: git clean will remove ALL untracked files, use with caution!"
