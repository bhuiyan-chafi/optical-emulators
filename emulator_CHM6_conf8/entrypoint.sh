#!/bin/bash
set -e

# If argument is startNetconfAgent.sh (legacy TFS command), discard it
if [ "$1" = "./startNetconfAgent.sh" ] || [ "$1" = "startNetconfAgent.sh" ]; then
    shift
fi

# Detect target XML (prioritize argument, environment variable, legacy mount, standard mount, then default baked-in XML)
if [ -n "$1" ] && [ -f "$1" ]; then
    TARGET_XML="$1"
elif [ -n "$XML_CONFIG" ] && [ -f "$XML_CONFIG" ]; then
    TARGET_XML="$XML_CONFIG"
elif [ -f "/confd/examples.confd/OC23/demoECOC21.xml" ]; then
    TARGET_XML="/confd/examples.confd/OC23/demoECOC21.xml"
elif [ -f "/my_project/init_config.xml" ]; then
    TARGET_XML="/my_project/init_config.xml"
else
    TARGET_XML="/my_project/init_chm6_config.xml"
fi

echo "=== Starting Nokia CHM6 ConfD Emulator ==="
echo "Selected initial configuration: $TARGET_XML"

# 1. Environment & directory setup
source /opt/confd/confdrc
mkdir -p /opt/confd/var/confd/log /opt/confd/etc/confd/ssh /opt/confd/var/confd/cdb

# Generate host RSA key if missing
if [ ! -f /opt/confd/etc/confd/ssh/ssh_host_rsa_key ]; then
    ssh-keygen -t rsa -f /opt/confd/etc/confd/ssh/ssh_host_rsa_key -N "" -q
    chmod 600 /opt/confd/etc/confd/ssh/ssh_host_rsa_key
fi

# 2. Start ConfD daemon
echo "Starting ConfD daemon..."
confd -c /my_project/etc/confd/confd.conf

# 3. Wait for ConfD IPC socket to be ready
echo "Waiting for ConfD IPC to initialize..."
until confd --status >/dev/null 2>&1; do
    sleep 0.2
done

# 4. Ingest configuration & operational state
if [ -f "$TARGET_XML" ]; then
    echo "Loading configuration and operational state from $TARGET_XML..."
    confd_load -l -m -o "$TARGET_XML"
    confd_load -l -m -O "$TARGET_XML"
    echo "ConfD CDB successfully initialized."
else
    echo "Warning: Configuration XML not found at $TARGET_XML"
fi

echo "=== Emulator Ready ==="
echo "NETCONF SSH listening on container port 2022"
echo "CLI SSH listening on container port 2024"

# 5. Keep container running and stream logs to stdout
exec tail -f /opt/confd/var/confd/log/confd.log
