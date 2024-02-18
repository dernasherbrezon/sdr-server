pipeline {
    agent none
    parameters {
        string(name: 'BASE_VERSION', defaultValue: '1.1', description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
        string(name: 'BUILD_NUMBER', defaultValue: '77', description: 'From https://github.com/dernasherbrezon/sdr-server/actions')
    }
    environment {
        GPG = credentials("GPG")
        DEBEMAIL = 'gpg@r2cloud.ru'
        DEBFULLNAME = 'r2cloud'
    }
    stages {
        stage('Package and deploy') {
            matrix {
                axes {
                    axis {
                        name 'OS_CODENAME'
                        values 'bullseye', 'stretch', 'buster', 'bookworm'
                    }
                    axis {
                        name 'CPU'
                        values 'nocpuspecific'
                    }
                }
                agent {
                    label "${OS_CODENAME}"
                }
                stages {
                    stage('Checkout') {
                        steps {
                            git(url: 'git@github.com:dernasherbrezon/sdr-server.git', branch: "${OS_CODENAME}", credentialsId: '5c8b3e93-0551-475c-9e54-1266242c8ff5', changelog: false)
                        }
                    }
                    stage('build and deploy') {
                        steps {
                            sh 'echo $GPG_PSW | /usr/lib/gnupg2/gpg-preset-passphrase -c C4646FB23638AE9010EB1F7F37A0505CF4C5B746'
                            sh 'echo $GPG_PSW | /usr/lib/gnupg2/gpg-preset-passphrase -c 9B66E29FF6DDAD62FA3F2570E02775B6EFAF6609'
                            sh "bash ./build_and_deploy.sh ${CPU} ${OS_CODENAME} ${params.BASE_VERSION} ${params.BUILD_NUMBER} ${GPG_USR}"
                        }
                    }
                }
            }
        }
    }
}
