#!/bin/sh
set -e

MDEC=/usr/mdec
BOOT=/boot_monitor
ROOT=`printroot -r`

if [ ! -b "$ROOT" ]
then
	echo root device $ROOT not found
	exit 1
fi

echo -n "Install boot as $BOOT on current root? (y/N) "
read ans

if [ ! "$ans" = y ]
then
	echo Aborting.
	exit 1
fi

echo "Installing boot monitor into $BOOT."
cp $MDEC/boot_monitor $BOOT

disk=`echo "$ROOT" | sed 's/s[0-3]//'`
echo -n "Install bootxx_minixfs3 into $disk? (y/N) "
read ans

if [ ! "$ans" = y ]
then
	echo Exiting...
	sync
	exit 0
fi

echo "Installing bootxx_minixfs3 into $disk."
installboot_nbsd "$disk" "$MDEC/bootxx_minixfs3"

sync
