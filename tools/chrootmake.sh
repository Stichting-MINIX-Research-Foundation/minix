#!/bin/sh
set -e
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
export SHELL=/bin/sh
cd /usr/src 
make etcfiles
su bin -c 'make world install' 
cd tools 
rm revision
rm /boot/image/*
make install 
cp /boot/image/* /boot/image_big  # Make big image accessible by this name
cp ../boot/boot /boot/boot 
make clean
make image_small 
cp image_small /boot 
cd /usr/src 
make clean
# Let man find the manpages
su bin -c 'makewhatis /usr/man'
su bin -c 'makewhatis /usr/gnu/man'
su bin -c 'makewhatis /usr/local/man'
mv /usr/src/commands /usr/src.commands
binsizes normal
exit 0

