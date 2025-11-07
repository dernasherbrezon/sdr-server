#!/usr/bin/env bash

set -e

BASE_VERSION=$1
BUILD_NUMBER=$2
OS_CODENAME=$3
OS_ARCHITECTURE=$4

. ./configure_flags.sh nocpuspecific

rm -rf build

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPACK_DEBIAN_PACKAGE_VERSION=${BASE_VERSION}.${BUILD_NUMBER}~${OS_CODENAME} -DCUSTOM_ARCHITECTURE=${OS_ARCHITECTURE} ..
VERBOSE=1 make -j $(nproc)
cpack
