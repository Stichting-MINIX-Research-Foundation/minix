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
if [ $MAKEMAP -ne 0 ]; then
	find . -type f -perm 755 | xargs nm -n 2> /dev/null > symbols.txt
fi
make clean
make cleandepend
find . -name obj-ack -type d|xargs rm -rf
# Let man find the manpages
makewhatis /usr/man
makewhatis /usr/local/man
binsizes normal
