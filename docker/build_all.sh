#!/usr/bin/env bash

docker buildx build --load --platform=armhf -t sdrserver-buster-armhf -f buster.Dockerfile .
docker buildx build --load --platform=armhf -t sdrserver-stretch-armhf -f stretch.Dockerfile .
docker buildx build --load --platform=armhf -t sdrserver-bullseye-armhf -f bullseye.Dockerfile .
docker buildx build --load --platform=armhf -t sdrserver-bookworm-armhf -f bookworm.Dockerfile .
docker buildx build --load --platform=arm64 -t sdrserver-bookworm-arm64 -f bookworm.Dockerfile .
docker buildx build --load --platform=armhf -t sdrserver-trixie-armhf -f trixie.Dockerfile .
docker buildx build --load --platform=arm64 -t sdrserver-trixie-arm64 -f trixie.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrserver-bionic-amd64 -f bionic.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrserver-focal-amd64 -f focal.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrserver-jammy-amd64 -f jammy.Dockerfile .
