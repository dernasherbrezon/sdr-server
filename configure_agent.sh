#!/bin/bash

set -e

sudo apt-get update
sudo apt-get install -y dirmngr lsb-release
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys A5A70917
sudo bash -c "echo \"deb http://r2cloud.s3.amazonaws.com $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud.list"
sudo bash -c "echo \"deb http://r2cloud.s3.amazonaws.com/cpu-generic $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud-generic.list"
sudo apt-get update
sudo apt-get install -y debhelper git-buildpackage cmake libconfig-dev valgrind check cmake pkg-config libvolk2-dev librtlsdr-dev

APT_CLI_VERSION="apt-cli-1.8"
if [ ! -f ${HOME}/${APT_CLI_VERSION}.jar ]; then
  wget -O ${APT_CLI_VERSION}.jar.temp https://github.com/dernasherbrezon/apt-cli/releases/download/${APT_CLI_VERSION}/apt-cli.jar
  mv ${APT_CLI_VERSION}.jar.temp ${HOME}/${APT_CLI_VERSION}.jar
fi
