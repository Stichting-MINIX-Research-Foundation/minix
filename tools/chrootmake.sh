#!/bin/sh
set -e
export SHELL=/bin/sh
cd /usr/src 
make etcfiles
make world
cd tools 
rm revision
rm /boot/image/*
make install
cp /boot/image/* /boot/image_big  # Make big image accessible by this name
cp ../boot/boot /boot/boot 
cd /usr/src 
make clean
# Let man find the manpages
makewhatis /usr/man
makewhatis /usr/local/man
binsizes normal
