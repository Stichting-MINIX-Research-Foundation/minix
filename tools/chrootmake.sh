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
cp ../boot/boot/boot /boot/boot

CC=clang make cleandepend clean depend image
rm revision
rm -rf /boot/minix/* # on old systems might not be present
CC=clang make install
cp ../sys/arch/i386/stand/boot/biosboot/boot_monitor /
cp -rf /boot/minix/* /boot/minix_default

cd /usr/src 
if [ $MAKEMAP -ne 0 ]; then
	find . -type f -perm 755 | xargs nm -n 2> /dev/null > symbols.txt
fi
make clean
make cleandepend
find . -name 'obj-*' -type d|xargs rm -rf
