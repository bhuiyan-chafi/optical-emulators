#!/bin/bash
set -e

XML_FILE="${1:-/my_project/init_pss32_config.xml}"

if [ ! -f "$XML_FILE" ]; then
    echo "Error: XML file not found at $XML_FILE"
    exit 1
fi

source /opt/confd/confdrc
echo "Pushing configuration and operational state from $XML_FILE into ConfD CDB..."
confd_load -l -m -o "$XML_FILE"
confd_load -l -m -O "$XML_FILE"
confd_cmd -c "creset"
echo "Configuration loaded and candidate datastore synchronized successfully."
