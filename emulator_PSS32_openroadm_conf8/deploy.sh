#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONTAINER_NAME="${1:-RDM1}"
CONTAINER_IP="${2:-10.10.102.11}"
IMAGE_NAME="nokia-pss32-openroadm-conf8"
DOCKERHUB_IMAGE="chafiullah/emu_pss32_openroadm"

cleanup() {
    rm -rf "$SCRIPT_DIR/build_context" "$SCRIPT_DIR/confd-basic-8.0.20.linux.x86_64.signed.zip"
}
trap cleanup EXIT

echo "=== Nokia 1830 PSS-32 (OpenROADM v18.1.0) Standalone Docker Image Build & Deploy ==="

# 1. Generate initial XML and stage FXS binaries + ConfD installer zip
echo "[1/4] Generating initial XML and preparing build context from ../emulator_ORDM_conf8..."
python3 generate_pss32_xml.py
mkdir -p build_context/confd-cdb
cp -L ../emulator_ORDM_conf8/my_project/confd-cdb/*.fxs build_context/confd-cdb/
cp ../emulator_TP_conf8/confd-basic-8.0.20.linux.x86_64.signed.zip ./

# 2. Build self-contained Docker image
echo "[2/4] Building standalone Docker images: $IMAGE_NAME:latest and $DOCKERHUB_IMAGE:latest..."
docker build -t "$IMAGE_NAME:latest" -t "$DOCKERHUB_IMAGE:latest" .

# Clean up staging files immediately after build
cleanup
echo "  -> Cleaned up staging build artifacts (zero YANG/FXS files stored in repository)."

# 3. Start emulator container
echo "[3/4] Launching standalone container $CONTAINER_NAME..."
bash run.sh "$CONTAINER_NAME" "$CONTAINER_IP"

# 4. Wait for ConfD to initialize
echo "[4/4] Waiting 5 seconds for ConfD to initialize and auto-load initial XML..."
sleep 5

if ! docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "Error: Container $CONTAINER_NAME exited unexpectedly. Startup logs:"
    docker logs "$CONTAINER_NAME"
    exit 1
fi

ACTUAL_IP="$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$CONTAINER_NAME")"

echo ""
echo "=== Deployment Complete ==="
echo "Container $CONTAINER_NAME running with IP $ACTUAL_IP (zero host port bindings)"
echo "NETCONF SSH: ssh -p 2022 -s admin@$ACTUAL_IP netconf (Password: admin)"
echo "CLI SSH:     ssh -p 2024 admin@$ACTUAL_IP (Password: admin)"
echo "Ready to push to Docker Hub with: docker push $DOCKERHUB_IMAGE:latest"
echo "Verify digital twin with: python3 client_verify.py $ACTUAL_IP 2022"
