#!/bin/sh
set -e
export SHELL=/bin/sh
cd /usr/src 

if [ $# -gt 0 ]
then	make $@
	exit $?
fi

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
