#!/bin/sh

TAG=`uname -p`-`uname -r`.`uname -v`
PACKDIR=`uname -p`/`uname -r`.`uname -v`
RC=/usr/etc/rc.package
CDDIR=PACKAGES
CDMP=/mnt
CDPACK=${CDMP}/install/packages
CDSRC=${CDMP}/install/package-sources
SRC=/usr/bigsrc
LISTFILE=/etc/packages-$TAG
LISTURL=http://www.minix3.org/packages/$PACKDIR/List
TMPDIR=/usr/tmp/packages
mkdir -p $TMPDIR
URL1=http://www.minix3.org/packages/$PACKDIR
SRCURL1=http://www.minix3.org/software
PATH=/bin:/sbin:/usr/bin:/usr/sbin
pack=""
cdpackages=""
netpackages=""
cdmounted=""

if [ ! "$PAGER" ]
then	PAGER=more
fi

if [ "$1" = -y ]
then	YESMODE=1
	PAGER=cat
fi

myread()
{
	if [ "$YESMODE" ]
	then	echo "all"
	else	read ans
		echo $ans
	fi
}

myexit()
{
	if [ -n "$cdmounted" -a -n "$pack" ]
	then
		umount $pack || true
	fi

	exit $1
}

# can we execute bunzip2?
if bunzip2 --help 2>&1 | grep usage >/dev/null
then    BUNZIP2=bunzip2 
else    BUNZIP2=smallbunzip2
fi

if id | fgrep "uid=0(" >/dev/null
then	:
else	echo "Please run $0 as root."
	myexit 1
fi

chmod 700 $TMPDIR

if [ -f "$RC" ]
then	. "$RC"
fi

cd /

# Make sure there is a $SRC dir
if [ ! -d "$SRC" ]
then	mkdir $SRC || myexit 1
fi

# Is there a usable CD to install packages from?
if [ -n "$cddrive" ]
then	pack=${cddrive}p2
	umount $pack >/dev/null 2>&1 || true
	echo "Checking for CD in $pack."
	if mount -r $pack $CDMP 2>/dev/null
	then	fn="$CDPACK/List"
		echo "Found."
		cdmounted=1
		cdpackages=$fn
		if [ ! -f $cdpackages ]
		then	cdpackages=""
			echo "No package list found on CD in $fn."
		fi
	else	echo "Not found."
	fi
else	echo "Don't know where the install CD is. You can set it in $RC."
fi

TMPF=$TMPDIR/.list.$$
rm -f $TMPF
rm -f $TMPDIR/.*	# Remove any remaining .postinstall script or .list*

# Check for network packages too
if ( : </dev/tcp ) 2>/dev/null
then	echo -n "Update package list from network? (Y/n) "
	y=`myread`
	if [ "$y" != n -a "$y" != N ]
	then	echo "Fetching package list from $LISTURL."
		urlget $LISTURL >$TMPF && mv $TMPF $LISTFILE || echo "Update not successful."
	fi
	netpackages=$LISTFILE
	if [ ! -f "$netpackages" -o ! `cat "$netpackages" 2>/dev/null | wc -l | awk '{ print $1 }'` -gt 1 ]
	then	netpackages=""
	fi
else	echo "No working network detected."
fi

# Is there at least one package type?
if [ ! -n "$netpackages" -a ! -n "$cdpackages"  ]
then	echo "No packages found."
	myexit 1
fi

# Is there more than one package type?
if [ -n "$netpackages" -a -n "$cdpackages"  ]
then	echo -n "Would you like to install from (C)D or (N)etwork? [C] "
	whichsrc=`myread`
	if [ "$whichsrc" = N -o "$whichsrc" = n ]
	then	unset cdpackages
	else	unset netpackages
	fi
fi

if [ -n "$netpackages" ]
then	source=net
else	source=cdrom
fi

cont=y
while [ "$cont" = y ]
do	cd $TMPDIR
	echo ""
	echo "Showing you a list of packages using $PAGER. Press q when"
	echo "you want to leave the list."
	echo -n "Press RETURN to continue.."
	xyzzy=`myread`
	echo "Package list:"
	(	echo "No.|Package|Description"
		(
		if [ -f "$netpackages" -a "$source" = net ]
		then	cat $netpackages
		fi
		if [ -f "$cdpackages" -a "$source" = cdrom ]
		then	cat $cdpackages
		fi
		) | sort -f -t'|' +0 | awk '{ n++; printf "%d|%s\n", n, $0 }' 
	) >$TMPF
	highest="`wc -l $TMPF | awk '{ print $1 - 1 }'`"
	awk -F'|' <$TMPF '{ printf "%3s %-15s %s\n", $1, $2, $3 }' | $PAGER
	echo "Format examples: '3', '3,6', '3-9', '3-9,11-15', 'all'"
	echo -n "Package(s) to install (RETURN or q to exit)? "
	packnolist=`myread`
	if [ "$packnolist" = "" -o "$packnolist" = "q" ]
	then	myexit 0
	fi
	if [ "$packnolist" = all ]
	then	packnolist=1-$highest
	fi
	IFS=','
	set $packnolist
	echo -n "Get source(s) too? (y/N) "
	getsources=`myread`
   for packrange in $packnolist
   do
	# Get a-b range.
	IFS='-'
	set $packrange
	start=$1
	if [ $# = 2 ]
	then	end=$2
	else	end=$1
	fi
	IFS=' '
      # use awk to make the range list
      for packno in `awk </dev/null "BEGIN { for(i=$start; i<=$end; i++) { printf \"%d \", i } }"`
      do
	ok=y
	pat="^$packno|"
	if [ "`grep -c $pat $TMPF`" -ne 1 ]
	then	if [ "$packno" ]
		then	echo "$packno: Wrong package number."
		fi
		ok=n
	fi
	if [ $ok = y ]
	then	
	packagename="`grep $pat $TMPF | awk -F'|' '{ print $2 }'`"
	file=$packagename.tar.bz2

	echo ""

	if [ -f $file -a ! "$YESMODE" ]
	then	echo "Skipping $file - it's already in $TMPDIR."
		echo "Remove that file if you want to re-retrieve and install this package."
	else
		case $source in
		net*)   echo "Retrieving $packno ($packagename) from primary location into $TMPDIR .."
			srcurl=""
			if urlget $URL1/$file >$file
			then	echo "Retrieved ok. Installing .."
				packit $file && echo Installed ok.
				srcurl=$SRCURL1/$file
			else	echo "Retrieval failed."
			fi
			if [ "$getsources" = y -o "$getsources" = Y ]
			then	(	cd $SRC || myexit 2
					srcfile=${packagename}-src.tar.bz2
					echo "Retrieving source from $srcurl .."
					urlget $srcurl >$srcfile || myexit 3
					echo "Source retrieved in $SRC/$srcfile."
					$BUNZIP2 -dc $srcfile | tar xf - >/dev/null || myexit 3
					echo "Source unpacked in $SRC."
				)
			fi
			;;
		cdrom*)
			if [ -f $CDPACK/$file ]
			then	echo "Installing from $CDPACK/$file .."
				packit $CDPACK/$file && echo Installed ok.
			else	echo "$CDPACK/$file not found."
			fi
			srcfile=$CDSRC/${packagename}.tar.bz2
			if [ -f $srcfile -a "$getsources" = y ]
			then 
					(	cd $SRC || myexit 2
						$BUNZIP2 -dc $srcfile | tar xf - || myexit 3
						echo "Source $srcfile unpacked in $SRC."
					)
			fi
			;;
		esac
	fi
	else	cont=n
	fi
     done # Iterate package range
   done # Iterate package range list
done

rm -f $TMPDIR/.*	# Remove any remaining .postinstall script or .list*
myexit 0
