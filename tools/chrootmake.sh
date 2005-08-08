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
# Put compiler and libraries on root (ramdisk if enabled)
cp /usr/lib/* /lib
cp /usr/lib/i386/* /lib/i386/
exit 0

