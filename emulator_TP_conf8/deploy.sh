#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
XML_SRC="../xml/terminal_openconfig.xml"
PROJECT_DIR="my_project"
XML_DEST="$PROJECT_DIR/init_config.xml"
CONTAINER_NAME="terminalemu-conf8"
IMAGE_NAME="terminalemu-conf8"

echo "=== deployment started ==="

# 1. Prepare Configuration File
echo "[1/5] Copying OpenConfig XML to project directory..."
if [ ! -f "$XML_SRC" ]; then
    echo "Error: Source XML not found at $XML_SRC"
    exit 1
fi
cp "$XML_SRC" "$XML_DEST"
echo "  -> Copied to $XML_DEST"

# 1.5 Prepare Docker Build Context (Copy ConfD Zip)
CONFD_ZIP_SRC="../repos/confd-basic-8.0.20.linux.x86_64.signed.zip"
CONFD_ZIP_DEST="confd-basic-8.0.20.linux.x86_64.signed.zip"

echo "[1.5/5] Copying ConfD Zip to build context..."
if [ ! -f "$CONFD_ZIP_SRC" ]; then
    echo "Error: ConfD Zip not found at $CONFD_ZIP_SRC"
    exit 1
fi
cp "$CONFD_ZIP_SRC" "$CONFD_ZIP_DEST"
echo "  -> Copied ConfD Zip."

# 2. Build Docker Image
echo "[2/5] Building Docker image..."
docker build -t "$IMAGE_NAME" .

# 3. Clean up old container
echo "[3/5] Cleaning up old container..."
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    docker rm -f "$CONTAINER_NAME"
fi

# 4. Start Emulator
echo "[4/5] Starting emulator..."
bash run.sh
echo "  -> Waiting 15 seconds for ConfD to initialize..."
sleep 15

# 5. Load Configuration
echo "[5/5] Loading configuration..."
# Load Config (ignore state with -o)
docker exec "$CONTAINER_NAME" bash -c "source /opt/confd/confdrc && confd_load -l -m -o /my_project/init_config.xml"
# Load State (operational transaction with -O) allows writing state data
docker exec "$CONTAINER_NAME" bash -c "source /opt/confd/confdrc && confd_load -l -m -O /my_project/init_config.xml"

echo "=== Deployment Complete ==="
echo "Verify functionality with: ssh -p 2024 -s admin@localhost netconf"
