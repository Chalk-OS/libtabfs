pipeline {
    agent { label 'alpine' }

    stages {
        stage('Prepare System') {
            sh 'apk update && apk add build-base'
        }
        stage('Build') {
            sh 'make release'
            sh 'mkdir dist'
            sh 'PREFIX=$(pwd)/dist make install'
            sh 'tar -cvf release.tar ./dist'
            archiveArtifacts artifacts: 'release.tar', followSymlinks: false
            archiveArtifacts artifacts: 'dist/libtabfs_inspect', followSymlinks: false
        }
    }
}