#!/usr/bin/env bash

#
#

# bailout on errors and echo commands.
set -xe
sudo apt-get -qq update

#DOCKER_SOCK="unix:///var/run/docker.sock"

#echo "DOCKER_OPTS=\"-H tcp://127.0.0.1:2375 -H $DOCKER_SOCK -s devicemapper\"" \
#    | sudo tee /etc/default/docker > /dev/null
#sudo service docker restart;
#sleep 5;
#sudo docker pull fedora:31;

#docker run --privileged -d -ti -e "container=docker"  \
#    -v /sys/fs/cgroup:/sys/fs/cgroup \
#    -v $(pwd):/opencpn-ci:rw \
#    fedora:31   /usr/sbin/init
#DOCKER_CONTAINER_ID=$(docker ps | grep fedora | awk '{print $1}')
#docker logs $DOCKER_CONTAINER_ID
#docker exec -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
#    "bash -xe /opencpn-ci/ci/generic-build-mingw.sh 31;
#         echo -ne \"------\nEND OPENCPN-CI BUILD\n\";"
#docker ps -a
#docker stop $DOCKER_CONTAINER_ID
#docker rm -v $DOCKER_CONTAINER_ID

#sudo apt-get install python3-pip python3-setuptools

DOCKER_SOCK="unix:///var/run/docker.sock"

echo "DOCKER_OPTS=\"-H tcp://127.0.0.1:2375 -H $DOCKER_SOCK -s devicemapper\"" \
    | sudo tee /etc/default/docker > /dev/null
sudo service docker restart;
sleep 5;

docker run --rm --privileged multiarch/qemu-user-static:register --reset

#docker run --rm -t multiarch/ubuntu-debootstrap:arm64-bionic uname -a
#Linux 28c784e9c7bc 4.4.0-101-generic #124~14.04.1-Ubuntu SMP Fri Nov 10 19:05:36 UTC 2017 aarch64 aarch64 aarch64 GNU/Linux

docker run --privileged -d -ti -e "container=docker"  \
    multiarch/ubuntu-debootstrap:armhf-bionic /bin/bash
    
    
    
DOCKER_CONTAINER_ID=$(sudo docker ps | grep armhf-bionic | awk '{print $1}')
echo $DOCKER_CONTAINER_ID 
docker exec -ti $DOCKER_CONTAINER_ID pwd
docker exec -ti $DOCKER_CONTAINER_ID ls

#sudo docker logs $DOCKER_CONTAINER_ID
#docker exec -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
#    "bash -xe /opencpn-ci/ci/generic-build-mingw.sh 31;
#         echo -ne \"------\nEND OPENCPN-CI BUILD\n\";"

#sudo docker exec -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
#    "bash -xe /opencpn-ci/ci/travis-build-debian.sh;
#         echo -ne \"------\nEND OPENCPN-CI BUILD\n\";"

docker exec -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
    "bash -xe ci/travis-build-debian.sh;
         echo -ne \"------\nEND OPENCPN-CI BUILD\n\";"

docker ps -a
docker stop $DOCKER_CONTAINER_ID
docker rm -v $DOCKER_CONTAINER_ID

sudo apt-get install python3-pip python3-setuptools
