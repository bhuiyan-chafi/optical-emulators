#!/bin/bash
cd $(dirname "$0")
cp /home/chafi/openconfig-terminal-device/repos/confd-basic-8.0.20.linux.x86_64.signed.zip .
docker build -t confd-rdm-img .
rm confd-basic-8.0.20.linux.x86_64.signed.zip
