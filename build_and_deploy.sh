#!/bin/bash

set -e

CPU=$1
OS_CODENAME=$2
BASE_VERSION=$3
BUILD_NUMBER=$4
GPG_KEYNAME=$5
APT_CLI_VERSION="apt-cli-1.8"

. ./configure_flags.sh ${CPU}

git config user.email "gpg@r2cloud.ru"
git config user.name "r2cloud"
git merge origin/main --no-edit

rm -f ../sdr-server_*
rm -f ../sdr-server-*
gbp dch --auto --debian-branch=${OS_CODENAME} --upstream-branch=main --new-version=${BASE_VERSION}.${BUILD_NUMBER}-${BUILD_NUMBER}~${OS_CODENAME} --git-author --distribution=unstable --commit  < /dev/null
git push --set-upstream origin ${OS_CODENAME}
gbp buildpackage --git-ignore-new --git-upstream-tag=${BASE_VERSION}.${BUILD_NUMBER} --git-keyid=${GPG_KEYNAME}

cd ..
java -jar ${HOME}/${APT_CLI_VERSION}.jar --url s3://${BUCKET} --component main --codename ${OS_CODENAME} --gpg-keyname ${GPG_KEYNAME} --gpg-arguments "--pinentry-mode,loopback" save --patterns ./*.deb,./*.ddeb
