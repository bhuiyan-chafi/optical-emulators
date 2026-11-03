#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONTAINER_NAME="TP1"
IMAGE_NAME="nokia-chm6-conf8"
DOCKERHUB_IMAGE="chafiullah/emu_chm6"

echo "=== Nokia CHM6 Standalone Docker Image Build & Deploy ==="

# 1. Prepare build context (stage FXS binaries and ConfD zip)
echo "[1/4] Preparing build context for self-contained image..."
mkdir -p build_context/confd-cdb
cp -L ../emulator_TP_conf8/my_project/confd-cdb/*.fxs build_context/confd-cdb/
cp ../emulator_TP_conf8/confd-basic-8.0.20.linux.x86_64.signed.zip ./

# 2. Build self-contained Docker image
echo "[2/4] Building standalone Docker images: $IMAGE_NAME and $DOCKERHUB_IMAGE:latest..."
docker build -t "$IMAGE_NAME:latest" -t "$DOCKERHUB_IMAGE:latest" .

# Clean up staging files immediately after build
rm -rf build_context confd-basic-8.0.20.linux.x86_64.signed.zip
echo "  -> Cleaned up staging build artifacts."

# 3. Start emulator container
echo "[3/4] Launching standalone container..."
bash run.sh

# 4. Wait for ConfD to initialize
echo "[4/4] Waiting 5 seconds for ConfD to initialize and auto-load Day-0 XML..."
sleep 5

echo ""
echo "=== Deployment Complete ==="
echo "Container running on network netbrTFS with IP 10.10.102.1"
echo "NETCONF SSH: ssh -p 2022 -s admin@10.10.102.1 netconf (Password: admin)"
echo "CLI SSH:     ssh -p 2024 admin@10.10.102.1 (Password: admin)"
echo "Ready to push to Docker Hub with: docker push $DOCKERHUB_IMAGE:latest"
echo "Verify digital twin with: python3 client_verify.py"
