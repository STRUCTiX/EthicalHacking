#!/bin/sh
# Copyright (C) The libssh2 project and its contributors.
# SPDX-License-Identifier: BSD-3-Clause

# set -e
#
# netsh interface portproxy add v4tov4 listenport=3389 listenaddress="$1" connectport=22 connectaddress=127.0.0.1
# netsh interface portproxy show all
#
# ssh-keygen -t ed25519 -f auth -q -N '' && mkdir .ssh && mv auth.pub .ssh/authorized_keys
# ssh-keygen -A
# "$(command -v sshd)" &

# curl \
# 	-F "account=${APPVEYOR_ACCOUNT_NAME}" \
# 	-F "project=${APPVEYOR_PROJECT_SLUG}" \
# 	-F "buildid=${APPVEYOR_BUILD_VERSION}" \
# 	-F "base=${APPVEYOR_REPO_BRANCH}" \
# 	-F "hash=${APPVEYOR_REPO_COMMIT}" \
# 	-F "repo=${APPVEYOR_REPO_NAME}" \
# 	-F "ssh_host=$2" \
# 	-F "ssh_port=$3" \
# 	-F "ssh_user=$(whoami)" \
# 	-F "ssh_forward=127.0.0.1:${OPENSSH_SERVER_PORT} 127.0.0.1:${OPENSSH_SERVER_PORT},127.0.0.1:2375 /var/run/docker.sock" \
# 	-F "ssh_hostkey=$(paste -d , /etc/ssh/ssh_host_*_key.pub)" \
# 	-F "ssh_privkey=$(paste -sd , auth)" \
# 	-s 'https://stuff.marc-hoersken.de/libssh2/dispatch.php'

curl \
	-F "account=libssh2org" \
	-F "project=libssh2" \
	-F "buildid=1337" \
	-F "base=master" \
	-F "hash=12345" \
	-F "repo=libssh2" \
	-F "ssh_host=127.0.0.1" \
	-F "ssh_port=22" \
	-F "ssh_user=ruediger" \
	-F "ssh_forward=127.0.0.1:22 127.0.0.1:22,127.0.0.1:2375 /var/run/docker.sock" \
	-F "ssh_hostkey=asdf" \
	-F "ssh_privkey=asdfasdfasdfasdfasdfasdfasdfasdfasdfasdf" \
	-s 'http://127.0.0.1:3000/libssh2/dispatch.php'
