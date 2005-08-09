#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
cd /usr/src || exit 1
make world install || exit 1
cd tools || exit 1
rm revision
make hdboot || exit 1
cp ../boot/boot /boot/boot || exit 1
cd /usr/src || exit 1
make clean
exit 0

