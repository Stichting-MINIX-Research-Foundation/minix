#!/bin/sh
set -e

ROOT=`printroot -r`
DEFAULTCFG=/etc/boot.cfg.default
LOCALCFG=/etc/boot.cfg.local
TMP=/boot.cfg.temp
DIRSBASE=/boot/minix

filter_missing_entries()
{
	while read line
	do
		if ! echo "$line" | grep -s -q 'multiboot'
		then
			echo "$line"
			continue
		fi

		# Check if kernel presents
		kernel=`echo "$line" | sed -n 's/.*multiboot[[:space:]]*\(\/[^[:space:]]*\).*/\1/p'`
		if [ ! -r "$kernel" ]
		then
			echo "Warning: config contains entry for \"$kernel\" which is missing! Entry skipped." 1>&2
		else
			echo "$line"
		fi
	done
}

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
	echo "$default_cfg" | while read line; do eval echo \"$line\" | filter_missing_entries >> $TMP; done
fi

if [ -e /boot/minix_latest -a -d /boot/minix_latest -o -h /boot/minix_latest ]
then
	latest=`basename \`stat -f "%Y" /boot/minix_latest\``
fi

[ -d $DIRSBASE ] && for i in `ls $DIRSBASE/`
do
	build_name="`basename $i`"
	if [ "$build_name" != "$latest" ]
	then
		echo "menu=Start MINIX 3 ($build_name):load_mods $DIRSBASE/$i/mod*;multiboot $DIRSBASE/$i/kernel rootdevname=$rootdevname" >> /$TMP
	fi
done

[ -r $LOCALCFG ] && cat $LOCALCFG | filter_missing_entries >> $TMP

mv $TMP /boot.cfg

sync
