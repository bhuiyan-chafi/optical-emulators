#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

XML_SRC="${1:-../sample_amplifier_xmls/edfa_preamp.xml}"
CONTAINER_NAME="confd-optical-amp"
IMAGE_NAME="optical-amp-conf8"
PROJECT_DIR="my_project"
XML_DEST="$PROJECT_DIR/init_config.xml"

echo "=== Optical Amplifier Emulator Deployment ==="
echo "Target XML configuration: $XML_SRC"

if [ ! -f "$XML_SRC" ]; then
    echo "Error: Source XML not found at $XML_SRC"
    exit 1
fi

echo "[1/4] Copying XML to $XML_DEST..."
cp "$XML_SRC" "$XML_DEST"

# Ensure image exists; tag terminalemu-conf8 if needed
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "[2/4] Tagging terminalemu-conf8:latest as $IMAGE_NAME:latest..."
    docker tag terminalemu-conf8:latest "$IMAGE_NAME:latest"
else
    echo "[2/4] Using existing image: $IMAGE_NAME"
fi

echo "[3/4] Starting container..."
bash run.sh
echo "  -> Waiting 5 seconds for ConfD to initialize..."
sleep 5

echo "[4/4] Loading configuration and operational state into ConfD CDB..."
docker exec "$CONTAINER_NAME" bash -c "source /opt/confd/confdrc && confd_load -l -m -o /my_project/init_config.xml"
docker exec "$CONTAINER_NAME" bash -c "source /opt/confd/confdrc && confd_load -l -m -O /my_project/init_config.xml"

echo "=== Optical Amplifier Emulator Ready! ==="
echo "NETCONF: ssh -p 2028 admin@localhost (Password: admin)"
echo "CLI:     ssh -p 2029 admin@localhost (Password: admin)"
echo "Verify:  python3 client_amp.py"
