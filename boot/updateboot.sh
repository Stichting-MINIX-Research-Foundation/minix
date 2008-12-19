#!/bin/sh
set -e

BOOT=/boot/boot
ROOT=`printroot -r`

if [ ! -b "$ROOT" ]
then	echo root device $ROOT not found
	exit 1
fi

echo -n "Install boot as $BOOT on current root and patch into $ROOT? (y/N) "
read ans

if [ ! "$ans" = y ]
then	echo Aborting.
	exit 1
fi

make install || true

echo Installing boot monitor into $BOOT.
cp boot $BOOT

echo Patching position of $BOOT into $ROOT.
installboot -d "$ROOT" /usr/mdec/bootblock $BOOT
sync
