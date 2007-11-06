#!/bin/sh
set -e
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
cd /usr/src 
make clean
# Let man find the manpages
su bin -c 'makewhatis /usr/man'
su bin -c 'makewhatis /usr/local/man'
binsizes normal
