#!/bin/bash

# Remove existing container if it exists
docker rm -f confd-rdm

# Run the container
# Mapping:
# Host 2026 -> Container 2022 (NETCONF)
# Host 2027 -> Container 2024 (CLI)
# Host 4567 -> Container 4565 (IPC)
docker run -d \
    --name confd-rdm \
    -p 2026:2022 \
    -p 2027:2024 \
    -p 4567:4565 \
    -v $(pwd)/emulator_RDM_conf8/my_project:/my_project \
    confd-ordm-img \
    confd --foreground --verbose -c /my_project/etc/confd/confd.conf
