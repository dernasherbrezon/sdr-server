#!/usr/bin/env bash

docker build --platform=armhf -t sdrserver-buster -f buster.Dockerfile .
docker build --platform=armhf -t sdrserver-stretch -f stretch.Dockerfile .