#!/bin/sh

RC=/usr/etc/rc.package
CDDIR=PACKAGES
CDMP=/mnt
CDPACK=${CDMP}/install/packages
CDSRC=${CDMP}/install/package-sources
SRC=/usr/src/commands
LISTFILE=/etc/packages
LISTURL=http://www.minix3.org/packages/List
TMPDIR=/usr/tmp/packages
mkdir -p $TMPDIR
URL1=http://www.minix3.org/packages
URL2=http://www.minix3.org/beta_packages
SRCURL1=http://www.minix3.org/software
SRCURL2=http://www.minix3.org/beta_software

if id | fgrep "uid=0(" >/dev/null
then	:
else	echo "Please run $0 as root."
	exit 1
fi

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

cdpackages=""
if [ -n "$cddrive" ]
then	pack=${cddrive}p2
	umount $pack >/dev/null 2>&1 || true
	if mount -r $pack $CDMP
	then	fn="$CDPACK/List"
		cdpackages=$fn
		if [ ! -f $cdpackages ]
		then	cdpackages=""
			echo "No package list found on CD in $fn."
		fi
	else	echo "CD mount failed."
	fi
else	echo "Don't know where the install CD is. You can set it in $RC."
fi

if [ "$cdpackages" = "" ]
then	echo "Skipping CD packages."
fi

TMPF=/tmp/list.$$

netpackages=""
if </dev/tcp
then	if [ -f $LISTFILE ]
	then	echo -n "Update package list from network? (y/N) "
		read y
	else	echo "No package list found - retrieving initial version."
		y=y
	fi
	if [ "$y" = y -o "$y" = Y ]
	then	echo "Fetching package list."
		urlget $LISTURL >$TMPF && mv $TMPF $LISTFILE || echo "Update not successful."
	fi
	netpackages=$LISTFILE
	if [ ! -f "$netpackages" -o ! `cat "$netpackages" 2>/dev/null | wc -l | awk '{ print $1 }'` -gt 1 ]
	then	netpackages=""
	fi
else	echo "No working network detected."
fi

if [ "$netpackages" = "" ]
then	echo "Skipping network packages."
	if [ "$cdpackages" = "" ]
	then	echo "No packages found."
		exit 1
	fi
fi

cont=y
while [ "$cont" = y ]
do	cd $TMPDIR
	echo ""
	echo "Showing you a list of packages using more. Press q when"
	echo "you want to leave the list."
	echo -n "Press RETURN to continue.."
	read xyzzy
	echo "Package list:"
	(	echo "No.|Source|Package|Description"
		(
		if [ -f "$netpackages" ]
		then	sed <$netpackages 's/^/net\|/'
		fi
		if [ -f "$cdpackages" ]
		then	sed <$cdpackages 's/^/cdrom\|/'
		fi
		) | sort -n -t'|' +2 | awk '{ n++; printf "%d|%s\n", n, $0 }'
	) >$TMPF
	awk -F'|' <$TMPF '{ printf "%3s %-6s %-15s %s\n", $1, $2, $3, $4 }' | more
	echo -n "Package to install? [RETURN for none] "
	read packno
	if [ -n "$packno" ]
	then	source="`grep "^$packno|" $TMPF | awk -F'|' '{ print $2 }'`"
		packagename="`grep "^$packno|" $TMPF | awk -F'|' '{ print $3 }'`"
		file=$packagename.tar.bz2
		echo -n "Get source of $packagename? (y/N) "
		read src
		case $source in
		net*)	echo "Retrieving binary from primary location into $TMPDIR .."
			srcurl=""
			if urlget $URL1/$file >$file
			then	packit $file && echo Installed ok.
				srcurl=$SRCURL1/$file
			else	echo "Retrying from Beta binary location.."
				if urlget $URL2/$file >$file
				then	packit $file  && echo Installed ok.
					srcurl=$SRCURL2/$file
				else echo "Retrieval failed."
				fi
			fi
			if [ "$src" = y -o "$src" = Y ]
			then	(	cd $SRC || exit
					srcfile=${packagename}-src.tar.bz2
					echo "Retrieving source from $srcurl .."
					urlget $srcurl >$srcfile || exit
					echo "Source retrieved in $SRC/$srcfile."
					smallbunzip2 -dc $srcfile | tar xf - >/dev/null || exit
					echo "Source unpacked in $SRC."
				)
			fi
			;;
		cdrom*)
			if -f [ $CDPACK/$file ]
			then	packit $CDPACK/$file
			fi
			if [ "$src" = y -o "$src" = Y ]
			then	(	cd $SRC || exit
					srcfile=$CDSRC/${packagename}-src.tar.bz2
					smallbunzip2 -dc $srcfile | tar xf - || exit
					echo "Source $srcfile unpacked in $SRC."
				)
			fi
			;;
		esac
	else	cont=n
	fi
done
