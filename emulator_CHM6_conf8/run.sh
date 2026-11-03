#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONTAINER_NAME="${1:-TP1}"
CONTAINER_IP="${2:-10.10.102.1}"
DOCKER_NET="${DOCKER_NET:-netbrTFS}"
IMAGE_NAME="chafiullah/emu_chm6:latest"

# Stop and remove existing container if running
if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "Stopping existing running container $CONTAINER_NAME..."
    docker exec "$CONTAINER_NAME" confd --stop >/dev/null 2>&1 || true
    sleep 1
fi
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "Removing existing container $CONTAINER_NAME..."
    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || docker rm "$CONTAINER_NAME" >/dev/null 2>&1 || true
fi

# Run container directly on Docker network with zero host port bindings (-p)
# TeraFlowSDN / clients connect directly to $CONTAINER_IP:2022
if docker network ls --format '{{.Name}}' | grep -q "^${DOCKER_NET}$"; then
    echo "Starting $CONTAINER_NAME on network $DOCKER_NET with IP $CONTAINER_IP (zero host port bindings)..."
    docker run -d \
        --net="$DOCKER_NET" \
        --ip="$CONTAINER_IP" \
        --name "$CONTAINER_NAME" \
        "$IMAGE_NAME"
else
    echo "Network $DOCKER_NET not found. Starting on default bridge network..."
    docker run -d \
        --name "$CONTAINER_NAME" \
        "$IMAGE_NAME"
fi

echo "Container $CONTAINER_NAME started successfully."
