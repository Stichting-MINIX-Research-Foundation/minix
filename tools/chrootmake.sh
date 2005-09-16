#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
cd /usr/src || exit 1
make etcfiles
su bin -c 'make world install' || exit 1
cd tools || exit 1
rm revision
rm /boot/image/*
make hdboot || exit 1
cp ../boot/boot /boot/boot || exit 1
make clean
make image_small || exit 1
cp image_small /boot || exit 1
cd /usr/src || exit 1
make clean
# Let man find the manpages
su bin -c 'makewhatis /usr/man'
su bin -c 'makewhatis /usr/gnu/man'
su bin -c 'makewhatis /usr/local/man'
mv /usr/src/commands /usr/src.commands
exit 0

