#!/bin/sh

set -e 

RC=/usr/etc/rc.package
CDDIR=PACKAGES
MP=/mnt
CDPACK=${MP}/install/packages
CDSRC=${MP}/install/package-sources
SRC=/usr/bigports
LISTFILE=/etc/packages
LISTURL=http://www.minix3.org/packages/List
TMPDIR=/usr/tmp/packages
mkdir -p $TMPDIR
URL1=http://www.minix3.org/packages
URL2=http://www.minix3.org/beta_packages

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

if [ -n "$cddrive" ]
then	pack=${cddrive}p2
	umount $pack >/dev/null 2>&1 || true
	if mount -r $pack $MP
	then
		cd $CDPACK
		for package in `ls *.tar.bz`
		do	grep $package List
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

TMPF=/tmp/list.$$

if </dev/tcp
then	if [ -f $LISTFILE ]
	then	echo -n "Update package list from network? (y/N) "
		read y
	else	y=y
	fi
	if [ "$y" = y -o "$y" = Y ]
	then	echo "Fetching package list."
		urlget $LISTURL >$TMPF && mv $TMPF $LISTFILE || echo Not done.
	fi
	cont=y
	while [ "$cont" = y ]
	do	cd $TMPDIR
		echo "Package list:"
		awk -F'|' <$LISTFILE '{ printf "%3s %-20s %s\n", $1, $2, $3 }' | more
		echo -n "Package to install? [RETURN for none] "
		read packno
		if [ -n "$packno" ]
		then	file="`grep "^$packno|" $LISTFILE | awk -F'|' '{ print $2 }'`"
			url=$file.tar.bz
			srcfile=$file-src.tar.bz
			if [ -n "$url" ]
			then	echo -n "Try to get source too? (y/N) "
				read src
				echo "Trying to fetch from $URL1/$url.."
				srcurl=$URL1/$srcfile
				if urlget $URL1/$url >$url
				then	echo Installing.
					packit $url
				else	echo "Trying to fetch from $URL2/$url.."
					srcurl=$URL2/$srcfile
					if urlget $URL2/$url >$url
					then	echo Installing Beta.
						packit $url
					else	echo "Retrieval failed."
					fi
				fi
				if [ "$src" = y -o "$src" = Y ]
				then	cd $SRC
					echo "Trying $srcurl"
					if urlget $srcurl >$srcfile
					then	echo "Extracting source into $SRC"
						smallbunzip2 -dc $srcfile | tar xf -
						echo "Done"
					else	echo "$srcurl not retrieved."
					fi
				fi
			else	echo "Package $packno not found."
			fi
			echo "Press RETURN to continue .."
			read xyzzy
		else	cont=n
		fi
	done
else	echo "No working network detected.
Please re-run this script with networking enabled to download
packages."
fi
