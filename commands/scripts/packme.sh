#!/bin/sh

set -e 

RC=/usr/etc/rc.package
CDDIR=PACKAGES

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

if [ -n "$cddrive" ]
then	for package in `isodir "$cddrive" $CDDIR | grep -i '\.tbz'`
	do	echo -n "Install $package (y/N) ? "
		read y
		if [ "$y" = y ]
		then	echo "Extracting $CDDIR/$package .."
			isoread "$cddrive" $CDDIR/$package | smallbunzip2 | pax -r -p e || echo "Extract failed."
		fi
	done
fi

