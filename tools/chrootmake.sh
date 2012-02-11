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
make install

make cleandepend clean depend image
rm revision
rm -rf /boot/minix/* # on old systems might not be present
make install
cp ../sys/arch/i386/stand/boot/biosboot/boot_monitor /
cp -rf /boot/minix/* /boot/minix_default

cd /usr/src 
if [ $MAKEMAP -ne 0 ]; then
	find . -type f -perm 755 | xargs nm -n 2> /dev/null > symbols.txt
fi
make clean
make cleandepend
