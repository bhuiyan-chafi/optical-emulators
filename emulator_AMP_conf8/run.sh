#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONTAINER_NAME="confd-optical-amp"
IMAGE_NAME="optical-amp-conf8"

# Remove existing container if it exists
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "Stopping and removing existing container $CONTAINER_NAME..."
    docker rm -f "$CONTAINER_NAME"
fi

# Run the container
# Port mapping:
# Host 2028 -> Container 2022 (NETCONF)
# Host 2029 -> Container 2024 (CLI)
# Host 4568 -> Container 4565 (IPC)
echo "Starting $CONTAINER_NAME container on host ports 2028 (NETCONF) and 2029 (CLI)..."
docker run -d \
    --name "$CONTAINER_NAME" \
    -p 2028:2022 \
    -p 2029:2024 \
    -p 4568:4565 \
    -v "$SCRIPT_DIR/my_project":/my_project \
    "$IMAGE_NAME" \
    confd --foreground --verbose -c /my_project/etc/confd/confd.conf

echo "Container $CONTAINER_NAME started successfully."
