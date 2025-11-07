pipeline {
    agent any
    parameters {
        string(name: 'BASE_VERSION', defaultValue: '1.1', description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
        string(name: 'BUILD_NUMBER', defaultValue: '77', description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
        choice(name: 'DOCKER_IMAGE', choices: ['debian:stretch-slim', 'debian:buster-slim', 'debian:bullseye-slim', 'debian:bookworm-slim', 'debian:trixie-slim', 'ubuntu:jammy', 'ubuntu:bionic', 'ubuntu:focal'], description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
        choice(name: 'OS_ARCHITECTURE', choices: ['armhf', 'arm64', 'amd64'], description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
    }

    stages {
        stage('Checkout') {
            steps {
                script {
                    env.OS_CODENAME = "${DOCKER_IMAGE}".split(':')[1].split('-')[0]
                    if( env.OS_CODENAME == "trixie" ) {
                        env.GPG_KEYNAME = '10E3EAED21238672'
                        env.GPG_FILE = 'gpg-sha512'
                        env.GPG_PASSPHRASE = 'gpg_passphrase-sha512'
                    } else {
                        env.GPG_KEYNAME = 'F2DCBFDCA5A70917'
                        env.GPG_FILE = 'gpg'
                        env.GPG_PASSPHRASE = 'gpg_passphrase'
                    }
                }
                git(url: 'git@github.com:dernasherbrezon/sdr-server.git', branch: "${OS_CODENAME}", credentialsId: 'github', changelog: false)
                sh '''
                    git config user.email "gpg@r2cloud.ru"
                    git config user.name "r2cloud"
                    git merge origin/main --no-edit
                '''
                withCredentials([sshUserPrivateKey(credentialsId: 'github', keyFileVariable: 'SSH_KEY')]) {
                    sh '''
                        GIT_SSH_COMMAND="ssh -i ${SSH_KEY}" git push --set-upstream origin ${OS_CODENAME}
                    '''
                }
            }
        }
        stage('build') {
            agent {
                docker {
                    image "sdrserver-${OS_CODENAME}-${OS_ARCHITECTURE}"
                    reuseNode true
                    args "--platform=linux/${OS_ARCHITECTURE}"
                }
            }
            steps {
                sh '''#!/bin/bash
                    set -e
                    . ./configure_flags.sh nocpuspecific
                    rm -rf build
                    mkdir build
                    cd build
                    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPACK_DEBIAN_PACKAGE_VERSION=${BASE_VERSION}.${BUILD_NUMBER}~${OS_CODENAME} -DCUSTOM_ARCHITECTURE=${OS_ARCHITECTURE} ..
                    VERBOSE=1 make -j $(nproc)
                    cpack
                '''
            }
        }
        stage('deploy') {
            steps {
                withCredentials([
                    file(credentialsId: "${GPG_FILE}", variable: 'gpg_file'),
                    file(credentialsId: "${GPG_PASSPHRASE}", variable: 'gpg_passphrase'),
                    aws(credentialsId: 'aws')]) {
                    sh '''#!/bin/bash
                        set -e
                        . ./configure_flags.sh nocpuspecific
                        APT_CLI_VERSION="apt-cli-1.11"
                        if [ ! -f ${HOME}/${APT_CLI_VERSION}.jar ]; then
                          wget -O ${APT_CLI_VERSION}.jar.temp https://github.com/dernasherbrezon/apt-cli/releases/download/${APT_CLI_VERSION}/apt-cli.jar
                          mv ${APT_CLI_VERSION}.jar.temp ${HOME}/${APT_CLI_VERSION}.jar
                        fi

                        java -jar ${HOME}/${APT_CLI_VERSION}.jar --url s3://${BUCKET} --component main --codename ${OS_CODENAME} --gpg-keyfile ${gpg_file} --gpg-keyname ${GPG_KEYNAME} --gpg-passphrase-file ${gpg_passphrase} save --patterns ./build/*.deb,./build/*.ddeb

                    '''
                }
            }
        }
        stage('test') {
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                    reuseNode true
                    args "--platform=linux/${OS_ARCHITECTURE} --pull always --user root"
                }
            }
            steps {
                sh '''#!/bin/bash
                    export DEBIAN_FRONTEND=noninteractive
                    cp ./docker/r2cloud.gpg /etc/apt/trusted.gpg.d/r2cloud.gpg
                    chmod 644 /etc/apt/trusted.gpg.d/r2cloud.gpg
                    if [ ${OS_CODENAME} == stretch ]; then
                        echo 'deb http://archive.debian.org/debian/ stretch main non-free contrib' > /etc/apt/sources.list
                        echo 'deb http://archive.debian.org/debian-security/ stretch/updates main non-free contrib' >> /etc/apt/sources.list
                    fi
                    echo "deb http://r2cloud.s3.amazonaws.com ${OS_CODENAME} main" >> /etc/apt/sources.list
                    echo "deb http://r2cloud.s3.amazonaws.com/cpu-generic ${OS_CODENAME} main" >> /etc/apt/sources.list
                    apt update && apt-get install --no-install-recommends -y sdr-server
                '''
            }
        }
    }
}
