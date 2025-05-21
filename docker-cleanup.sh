#!/bin/bash
# Script to clean up Docker containers, images, and volumes related to the UDP-to-RabbitMQ Gateway

echo "UDP-to-RabbitMQ Gateway Docker Cleanup Script"
echo "============================================"
echo "This script will stop and remove Docker containers, images, and volumes related to this project."
echo ""

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed or not in PATH"
    exit 1
fi

echo "Step 1: Stopping and removing containers..."
# Stop and remove any running containers related to our project
CONTAINERS=$(docker ps -a --filter "name=udp-gateway" --filter "name=redis" --filter "name=rabbitmq" --format "{{.ID}}")
if [ -n "$CONTAINERS" ]; then
    echo "Stopping containers: $CONTAINERS"
    docker stop $CONTAINERS
    echo "Removing containers: $CONTAINERS"
    docker rm $CONTAINERS
    echo "✓ Containers stopped and removed"
else
    echo "✓ No related containers found"
fi

echo "Step 2: Removing Docker images..."
# Remove related images
IMAGES=$(docker images "udp-gateway" --format "{{.ID}}")
if [ -n "$IMAGES" ]; then
    echo "Removing images: $IMAGES"
    docker rmi $IMAGES
    echo "✓ Images removed"
else
    echo "✓ No related images found"
fi

echo "Step 3: Removing Docker volumes..."
# Remove related volumes
VOLUMES=$(docker volume ls --filter "name=udp-to-rabbitmq-gateway" --format "{{.Name}}")
if [ -n "$VOLUMES" ]; then
    echo "Removing volumes: $VOLUMES"
    docker volume rm $VOLUMES
    echo "✓ Volumes removed"
else
    echo "✓ No related volumes found"
fi

echo "Step 4: Checking for stale networks..."
# Remove any networks that might be related
NETWORKS=$(docker network ls --filter "name=gateway-network" --format "{{.ID}}")
if [ -n "$NETWORKS" ]; then
    echo "Removing networks: $NETWORKS"
    docker network rm $NETWORKS
    echo "✓ Networks removed"
else
    echo "✓ No related networks found"
fi

echo "Step 5: Pruning unused Docker resources..."
read -p "Do you want to prune unused Docker resources? This will remove all unused containers, networks, and dangling images (y/n): " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Pruning Docker system..."
    docker system prune -f
    echo "✓ Docker system pruned"
else
    echo "× Docker system prune skipped"
fi

echo ""
echo "Docker Cleanup Complete!"
echo "All Docker resources related to the UDP-to-RabbitMQ Gateway have been removed."
echo ""
echo "To check for any remaining containers: docker ps -a"
echo "To check for any remaining images: docker images"
echo "To check for any remaining volumes: docker volume ls"
