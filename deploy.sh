#!/usr/bin/env bash

set -e

OS_CODENAME=$1

if [ "$OS_CODENAME" = "trixie" ]; then
  GPG_KEYNAME='10E3EAED21238672'
  GPG_FILE="${HOME}/secrets/private-sha512.gpg"
  GPG_PASSPHRASE="${HOME}/secrets/private-sha512.txt"
else
  GPG_KEYNAME='F2DCBFDCA5A70917'
  GPG_FILE="${HOME}/secrets/private.gpg"
  GPG_PASSPHRASE="${HOME}/secrets/private.txt"
fi

. ./configure_flags.sh nocpuspecific

APT_CLI_VERSION="apt-cli-1.11"
if [ ! -f ${HOME}/${APT_CLI_VERSION}.jar ]; then
  wget -O ${APT_CLI_VERSION}.jar.temp https://github.com/dernasherbrezon/apt-cli/releases/download/${APT_CLI_VERSION}/apt-cli.jar
  mv ${APT_CLI_VERSION}.jar.temp ${HOME}/${APT_CLI_VERSION}.jar
fi

java -jar ${HOME}/${APT_CLI_VERSION}.jar --url s3://${BUCKET} --component main --codename ${OS_CODENAME} --gpg-keyfile ${GPG_FILE} --gpg-keyname ${GPG_KEYNAME} --gpg-passphrase-file ${GPG_PASSPHRASE} save --patterns ./build/*.deb,./build/*.ddeb
