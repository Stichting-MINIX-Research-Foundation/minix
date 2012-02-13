#!/bin/sh
set -e

ROOT=`printroot -r`
DEFAULTCFG=/etc/boot.cfg.default
LOCALCFG=/etc/boot.cfg.local
TMP=/boot.cfg.temp

if [ ! -b "$ROOT" ]
then
	echo root device $ROOT not found
	exit 1
fi

rootdevname=`echo $ROOT | sed 's/\/dev\///'`

if [ -r $DEFAULTCFG ]
then
	default_cfg=`cat $DEFAULTCFG`
	# Substitute variables like $rootdevname
	echo "$default_cfg" | while read line; do eval echo \"$line\" >> $TMP; done
fi

latest=`basename \`stat -f "%Y" /boot/minix_latest\``

for i in /boot/minix/*
do
	build_name="`basename $i`"
	if [ "$build_name" != "$latest" ]
	then
		echo "menu=Start MINIX 3 ($build_name):load_mods $i/mod*;multiboot $i/kernel rootdevname=$rootdevname" >> /$TMP
	fi
done

[ -r $LOCALCFG ] && cat $LOCALCFG >> $TMP

mv $TMP /boot.cfg

sync
