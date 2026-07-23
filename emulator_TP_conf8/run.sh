#!/bin/bash
# Run the ConfD container
# - Maps port 2024 (NETCONF local) to 2024
# - Maps port 4565 (IPC)
# - Mounts the local project directory
# - Runs 'make start' inside the container to compile and start 

docker run -d \
    --name confd-emulator \
    -p 2024:2022 \
    -p 2025:2024 \
    -p 4565:4565 \
    -v $(pwd)/my_project:/my_project \
    confd-latest-img \
    /bin/bash -c "source /opt/confd/confdrc && mkdir -p /opt/confd/etc/confd/ssh /opt/confd/var/confd/log/confd && cp etc/confd/ssh_host_rsa_key /opt/confd/etc/confd/ssh/ssh_host_rsa_key && chmod 600 /opt/confd/etc/confd/ssh/ssh_host_rsa_key && make stop >/dev/null 2>&1; make all && /opt/confd/bin/confd -c etc/confd/confd.conf --foreground --verbose --addloadpath /my_project/confd-cdb"
