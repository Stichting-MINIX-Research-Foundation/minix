#!/bin/sh

set -e 

RC=/usr/etc/rc.package
CDDIR=PACKAGES
MP=/mnt
CDPACK=${MP}/install/packages

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
			fi
		done
	else	echo "CD mount failed - skipping CD packages."
	fi
else	echo "Don't know where the install CD is."
fi

