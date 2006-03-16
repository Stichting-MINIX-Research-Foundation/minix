#!/bin/sh

set -e 

RC=/usr/etc/rc.package
CDDIR=PACKAGES
MP=/mnt
CDPACK=${MP}/install/packages
CDSRC=${MP}/install/package-sources
SRC=/usr/bigports

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

if [ -n "$cddrive" ]
then	pack=${cddrive}p2
	umount $pack >/dev/null 2>&1 || true
	if mount $pack $MP
	then
		cd $CDPACK
		for package in `ls *.tar.bz`
		do	echo $package
			grep $package List
			echo -n "Install $package (y/N) ? "
			read y
			if [ "$y" = y -o "$y" = Y ]
			then	echo "Extracting $CDPACK/$package .."
				cat $package | packit -
				srcname="`echo $package | sed 's/.tar.bz/-src.tar.bz'`"
				srcarc="$CDSRC/$srcname"
				if [ -f "$srcarc" ]
				then	echo -n "Install its source (y/N) ? "
					read y
					if [ "$y" = y -o "$y" = Y ]
					then	echo "Installing $srcarc into $SRC."
						smallbunzip2 "$srcarc" | (cd $SRC && tar xf - )
					fi
				fi
			fi
		done
	else	echo "CD mount failed - skipping CD packages."
	fi
else	echo "Don't know where the install CD is."
fi

