#!/bin/bash

# Remove existing container
docker rm -f confd-ordm

# Run container
# NETCONF: Host 2028 -> Container 2022
# CLI: Host 2029 -> Container 2024
# IPC: Host 4568 -> Container 4565
docker run -d \
    --name confd-ordm \
    -p 2028:2022 \
    -p 2029:2024 \
    -p 4568:4565 \
    -v $(pwd)/emulator_ORDM_conf8/my_project:/my_project \
    confd-ordm-img \
    sh -c "cp /my_project/confd-cdb/*.xml /opt/confd/var/confd/cdb/ 2>/dev/null; confd --foreground --verbose -c /my_project/etc/confd/confd.conf"
