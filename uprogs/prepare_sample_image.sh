#! /usr/bin/env bash

DIR="$(dirname "$0")"
cd "${DIR}/fsroot"

IMGDIR="image/sample"
mkdir -p "${IMGDIR}"

for i in container_init cp cat chroot echo grep ifconfig kill ln ls mkdir ps pwd rm sh tcpechoserver udpechoserver unshare wc zombie; do
    cp "${i}" "$IMGDIR/${i}"
done