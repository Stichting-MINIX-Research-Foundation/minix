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

# can we execute bunzip2?
if bunzip2 --help 2>&1 | grep usage >/dev/null
then    BUNZIP2=bunzip2 
else    BUNZIP2=smallbunzip2
fi

if id | fgrep "uid=0(" >/dev/null
then	:
else	echo "Please run $0 as root."
	exit 1
fi

chmod 700 $TMPDIR

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

# For local testing
#cdpackages=/usr/bigports/Packages/List
#CDPACK=/usr/bigports/Packages
#CDSRC=/usr/bigports/Sources

if [ "$cdpackages" = "" ]
then	echo "Skipping CD packages."
else	cont=y
	while [ $cont = y ]
	do	n="`wc -l $cdpackages | awk '{ print $1 }'`"
		sourcef=$CDSRC/SizeMB
		binf=$CDPACK/SizeMB
		if [ -f $binf -a -f $sourcef ]
		then	sourcemb="`cat $sourcef`"
			binmb="`cat $binf`"
			sourcesize=" (`expr $binmb + $sourcemb` MB uncompressed)"
		else	sourcesize=""
		fi
		if [ -f $binf ]
		then	binmb="`cat $binf`"
			binsize=" ($binmb MB uncompressed)"
		else	binsize=""
		fi

		echo "There are $n CD packages."
		echo "Please choose:"
		echo " 1  Install all $n binary packages$binsize from CD"
		echo " 2  Install all $n binary packages + sources from CD$sourcesize"
		echo " 3  Display the list of packages on CD"
		echo " 4  Let me select individual packages to install from CD or network."
		echo " 5  Exit."
		echo -n "Choice: [4] "
		read in
		case "$in" in
		1|2)
			cd $CDPACK || exit
			echo " * Installing binaries .."
			for f in *.tar.bz2
			do	echo "Installing $f binaries .."
				packit $f && echo Installed $f
			done
			if [ "$in" = 2 ]
			then
				cd $SRC || exit
				echo " * Installing sources in $SRC .."
				for f in $CDSRC/*.tar.bz2
				do	echo "$f .."
					$BUNZIP2 -dc $f | tar xf - 
				done
			fi
			;;
		3)
			( echo "Displaying list; press q to leave it, space for more."
			  cat "$CDPACK/List" | awk -F'|' '{ printf "%-20s %s\n", $1, $2 }'
			) | more
			;;
		""|4)
			echo "Ok, showing packages to install." ; echo
			cont=n
			;;
		5)
			exit 0
			;;
		esac
	done
	echo -n "Press RETURN to continue .. "
	read xyzzy
fi

TMPF=$TMPDIR/.list.$$
rm -f $TMPF
rm -f $TMPDIR/.*	# Remove any remaining .postinstall script or .list*

netpackages=""
if ( : </dev/tcp ) 2>/dev/null
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
		) | sort -t'|' +1 | awk '{ n++; printf "%d|%s\n", n, $0 }' 
	) >$TMPF
	awk -F'|' <$TMPF '{ printf "%3s %-6s %-15s %s\n", $1, $2, $3, $4 }' | more
	echo -n "Package to install? [RETURN for none] "
	read packno
	ok=y
	pat="^$packno|"
	if [ "`grep $pat $TMPF | wc -l | awk '{ print $1 }'`" -ne 1 ]
	then	if [ "$packno" ]
		then	echo "Wrong package number."
		fi
		ok=n
	fi
	if [ $ok = y ]
	then	source="`grep $pat $TMPF | awk -F'|' '{ print $2 }'`"
		packagename="`grep $pat $TMPF | awk -F'|' '{ print $3 }'`"
		file=$packagename.tar.bz2
		case $source in
		net*)	echo -n "Get source of $packagename? (y/N) "
			read src
			echo "Retrieving binary from primary location into $TMPDIR .."
			srcurl=""
			if urlget $URL1/$file >$file
			then	echo "Retrieved ok. Installing .."
				packit $file && echo Installed ok.
				srcurl=$SRCURL1/$file
			else	echo "Retrying from Beta binary location.."
				if urlget $URL2/$file >$file
				then	echo "Retrieved ok. Installing .."
					packit $file  && echo Installed ok.
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
					$BUNZIP2 -dc $srcfile | tar xf - >/dev/null || exit
					echo "Source unpacked in $SRC."
				)
			fi
			;;
		cdrom*)
			if [ -f $CDPACK/$file ]
			then	echo "Installing from $CDPACK/$file .."
				packit $CDPACK/$file
			else	echo "$CDPACK/$file not found."
			fi
			srcfile=$CDSRC/${packagename}-src.tar.bz2
			if [ -f $srcfile ]
			then
				echo -n "Get source of $packagename? (y/N) "
				read src
				if [ "$src" = y -o "$src" = Y ]
				then	(	cd $SRC || exit
						$BUNZIP2 -dc $srcfile | tar xf - || exit
						echo "Source $srcfile unpacked in $SRC."
					)
				fi
			else	echo "No source on CD for $packagename."
			fi
			;;
		esac
	else	cont=n
	fi
done

rm -f $TMPDIR/.*	# Remove any remaining .postinstall script or .list*
