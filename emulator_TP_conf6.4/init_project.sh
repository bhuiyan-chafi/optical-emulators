#!/bin/bash
# Initialize a new confd project inside the container
# This project will be created in the mounted volume 'emulator/my_project'

docker run --rm \
    -v $(pwd)/emulator/my_project:/my_project \
    my-confd-img \
    /bin/bash -c "source /opt/confd/confdrc && \
    mkdir -p yang private-jar java-jar confd-cdb etc/confd && \
    cp /opt/confd/etc/confd/confd.conf etc/confd/confd.conf && \
    echo 'Project structure created manually.'"
