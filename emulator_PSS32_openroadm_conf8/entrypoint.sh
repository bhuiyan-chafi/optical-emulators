#!/bin/bash
set -e

# If argument is startNetconfAgent.sh (legacy TFS command), discard it
if [ "$1" = "./startNetconfAgent.sh" ] || [ "$1" = "startNetconfAgent.sh" ]; then
    shift
fi

# Detect target XML (prioritize argument, environment variable, standard mount, then default baked-in initial XML)
if [ -n "$1" ] && [ -f "$1" ]; then
    TARGET_XML="$1"
elif [ -n "$XML_CONFIG" ] && [ -f "$XML_CONFIG" ]; then
    TARGET_XML="$XML_CONFIG"
elif [ -f "/my_project/init_config.xml" ]; then
    TARGET_XML="/my_project/init_config.xml"
else
    TARGET_XML="/my_project/init_pss32_config.xml"
fi

echo "=== Starting Nokia 1830 PSS-32 OpenROADM v18.1.0 ConfD Emulator ==="
echo "Selected initial configuration: $TARGET_XML"

# 1. Environment & directory setup
source /opt/confd/confdrc
mkdir -p \
    /opt/confd/var/confd/log \
    /opt/confd/etc/confd/ssh \
    /opt/confd/var/confd/cdb \
    /opt/confd/var/confd/state \
    /opt/confd/var/confd/candidate \
    /opt/confd/var/confd/rollback

# Generate host RSA key if missing
if [ ! -f /opt/confd/etc/confd/ssh/ssh_host_rsa_key ]; then
    ssh-keygen -t rsa -f /opt/confd/etc/confd/ssh/ssh_host_rsa_key -N "" -q
    chmod 600 /opt/confd/etc/confd/ssh/ssh_host_rsa_key
fi

# Seed mandatory OpenROADM v18.1.0 top-level leaves in CDB before initial ConfD boot if CDB is uninitialized
if [ ! -f /opt/confd/var/confd/cdb/A.cdb ] && [ ! -f /opt/confd/var/confd/cdb/ordm_seed_init.xml ]; then
    cat > /opt/confd/var/confd/cdb/ordm_seed_init.xml << 'EOF'
<config xmlns="http://tail-f.com/ns/config/1.0">
  <ltp-template xmlns="http://org/openroadm/ltp-template">
    <vendor>Nokia</vendor>
  </ltp-template>
  <org-openroadm-device xmlns="http://org/openroadm/device">
    <info>
      <node-type>rdm</node-type>
      <node-subtype>none</node-subtype>
    </info>
  </org-openroadm-device>
</config>
EOF
fi

# 2. Start ConfD daemon
echo "Starting ConfD daemon..."
confd -c /my_project/etc/confd/confd.conf

# 3. Wait for ConfD IPC socket to be ready
echo "Waiting for ConfD IPC to initialize..."
until confd --status >/dev/null 2>&1; do
    sleep 0.2
done

# 4. Ingest initial configuration & operational state, then sync candidate datastore
if [ -f "$TARGET_XML" ]; then
    echo "Loading initial configuration and operational state from $TARGET_XML..."
    confd_load -l -m -o "$TARGET_XML"
    confd_load -l -m -O "$TARGET_XML"
    confd_cmd -c "creset"
    echo "ConfD CDB and candidate datastore successfully initialized."
else
    echo "Warning: Initial configuration XML not found at $TARGET_XML"
fi

echo "=== Emulator Ready ==="
echo "NETCONF SSH listening on container port 2022"
echo "CLI SSH listening on container port 2024"

# 5. Keep container running and stream logs to stdout
exec tail -f /opt/confd/var/confd/log/confd.log
