#!/bin/sh

set -e 

RC=/usr/etc/rc.package
CDDIR=PACKAGES

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

if [ -n "$cddrive" ]
then	isodir "$cddrive" $CDDIR | grep -i tbz | while read package
	do	echo -n "Install $package (y/n) ? "
		read y
		if [ "$y" = y ]
		then	isoread "$cddrive" $CDDIR/$package | bzip2 -d | pax -r -p e '*' / || echo "Extract failed."
		fi
	done
fi
