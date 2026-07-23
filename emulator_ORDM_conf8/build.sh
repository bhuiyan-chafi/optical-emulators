#!/bin/bash
cd $(dirname "$0")
# Use absolute path to the repo zip
cp /home/chafi/openconfig-terminal-device/repos/confd-basic-8.0.20.linux.x86_64.signed.zip .
docker build -t confd-ordm-img .
rm confd-basic-8.0.20.linux.x86_64.signed.zip
