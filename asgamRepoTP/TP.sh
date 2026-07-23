# run with sudo: sudo ./teraFlowTopo.sh
#!/bin/bash

#Remove previous topology
sudo docker stop TP1
sudo docker rm TP1

echo "Creating TP1 on 10.100.101.10"
docker run --net=netbr0 --ip=10.100.101.10 --name TP1 -v "./transponders_x4.xml:/confd/examples.confd/OC23/demoECOC21.xml" -dt asgamb1/oc23bgp.img ./startNetconfAgent.sh
sleep 2
