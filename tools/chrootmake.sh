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
rm -f /boot/kernel/* # on old systems might not be present
rm -rf /boot/modules/* # same as above
make install
cp /boot/image/* /boot/image_big  # Make big image accessible by this name
cp ../boot/boot/boot /boot/boot 
cp ../sys/arch/i386/stand/boot/biosboot/boot_monitor /
CC=clang make cleandepend clean depend image
CC=clang make install
cp /boot/kernel/* /boot/kernel_default
cp -rf /boot/modules/* /boot/modules_default
cd /usr/src 
if [ $MAKEMAP -ne 0 ]; then
	find . -type f -perm 755 | xargs nm -n 2> /dev/null > symbols.txt
fi
make clean
make cleandepend
find . -name 'obj-*' -type d|xargs rm -rf
