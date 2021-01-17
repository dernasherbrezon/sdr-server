#!/bin/bash

set -e 

mkdir debug && cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
build-wrapper-linux-x86-64 --out-dir bw-output make all
./run_tests.sh
make coverage
cd ..
sonar-scanner