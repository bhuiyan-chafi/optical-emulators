#!/bin/bash
set -e

cd "$(dirname "$0")"

# Reuse the ConfD zip from ../repos/
CONFD_ZIP="../repos/confd-basic-8.0.20.linux.x86_64.signed.zip"
if [ ! -f "$CONFD_ZIP" ]; then
    CONFD_ZIP="/home/chafi/openconfig-terminal-device/repos/confd-basic-8.0.20.linux.x86_64.signed.zip"
fi

if [ -f "$CONFD_ZIP" ]; then
    echo "Copying ConfD zip package from $CONFD_ZIP..."
    cp "$CONFD_ZIP" confd-basic-8.0.20.linux.x86_64.signed.zip
    echo "Building optical-amp-conf8 Docker image..."
    docker build -t optical-amp-conf8 .
    rm confd-basic-8.0.20.linux.x86_64.signed.zip
    echo "Build complete: optical-amp-conf8"
else
    echo "ConfD zip not found in ../repos/. Using existing terminalemu-conf8 image..."
    docker tag terminalemu-conf8:latest optical-amp-conf8:latest
    echo "Tagged terminalemu-conf8:latest -> optical-amp-conf8:latest"
fi
