#!/usr/bin/env bash

set -e

BASE_VERSION=$1
BUILD_NUMBER=$2
OS_CODENAME=$3
OS_ARCHITECTURE=$4

docker run --name sdrserver -t -d -u $(id -u):$(id -g)  --platform=linux/${OS_ARCHITECTURE} -w $(pwd) -v $(pwd):$(pwd) sdrserver-${OS_CODENAME}-${OS_ARCHITECTURE}
docker exec -w $(pwd) sdrserver bash ./build.sh $@
docker stop --time=1 sdrserver
docker rm sdrserver
