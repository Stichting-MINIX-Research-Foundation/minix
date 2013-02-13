#!/bin/sh

set -e

. release.functions

version_pretty="`sh ../sys/conf/osrelease.sh`"
version="`echo $version_pretty | tr . _`"
PACKAGEDIR=/usr/pkgsrc/packages/$version_pretty/`uname -m`

SRC=src
: ${REPO:=git://git.minix3.org/minix}
: ${GITBRANCH:=master}

# List of packages included on installation media
PACKAGELIST=packages.install
secs=`expr 32 '*' 64`
export SHELL=/bin/sh

PKG_ADD=/usr/pkg/sbin/pkg_add
PKG_INFO=/usr/pkg/sbin/pkg_info

if [ ! -x $PKG_ADD ]
then	echo Please install pkg_install from pkgsrc.
	exit 1
fi

# Packages we have to pre-install, and url to use
PACKAGEURL=ftp://ftp.minix3.org/pub/minix/packages/$version_pretty/`uname -m`/All/
PREINSTALLED_PACKAGES="
	pkgin
	pkg_install
	bmake
	binutils
	clang
	"
#	pkg_tarup

PKG_ADD_URL=$PACKAGEURL

RELEASERC=$HOME/.releaserc

if [ -f $RELEASERC ]
then	. $RELEASERC
fi

set -- $* $RELOPTS

export RELEASEDIR=/usr/r-staging
RELEASEMNTDIR=/usr/r

IMAGE=/usr/mdec/bootxx_cd9660
ROOTIMAGE=rootimage
CDFILES=/usr/tmp/cdreleasefiles
IMG_BASE=minix${version}_ide
BS=4096

HDEMU=0
COPY=0
JAILMODE=0
REVTAG=""
PACKAGES=1
MINIMAL=0
MAKEMAP=0
EXTRAS_INSTALL=0
EXTRAS_PATH=

# Do we have git?
if git --version >/dev/null
then	if [ -d ../.git ]
	then	LOCAL_REVTAG="`git describe --always`"
		GITMODE=1
	fi
fi

FILENAMEOUT=""

while getopts "b:j:ls:pmMchu?f:L:e:" c
do
	case "$c" in
	\?)
		echo "Usage: $0 [-l] [-p] [-c] [-h] [-m] [-M] [-u] [-f <filename>] [-s <username>] -j<jaildir> [-L <packageurl>] [-e <extras-path>]" >&2
		exit 1
	;;
	b)
		GITBRANCH=$OPTARG
		;;
	h)
		echo " * Making HD image"
		IMG_BASE=minix${version}_bios
		HDEMU=1
		;;
	c)
		echo " * Copying, not using GIT"
		COPY=1
		;;
	p)
		PACKAGES=0
		;;
	j)
		RELEASEDIR=$OPTARG
		JAILMODE=1
		;;
	u)
		echo " * Making live USB-stick image"
		IMG_BASE=minix${version}_usb
		HDEMU=1
		USB=1
		;;
	f)
		FILENAMEOUT="$OPTARG"
		;;
	s)	USERNAME="--username=$OPTARG"
		;;
	m)	MINIMAL=1
		PACKAGES=0
		;;
	M)	MAKEMAP=1
		;;
	l)	PKG_ADD_URL=file://$PACKAGEDIR/All
		;;
	L)	PKG_ADD_URL="$OPTARG"
		CUSTOM_PACKAGES=1
		;;
	e)	EXTRAS_INSTALL=1
		EXTRAS_PATH="$OPTARG"
		;;
	esac
done

RELEASEPACKAGE=${RELEASEDIR}/usr/install/packages

if [ $GITMODE -ne 1 -a $COPY -ne 1 ]
then	echo "Need git to retrieve latest minix! Copying src instead!"
	COPY=1
fi

if [ ! "$ZIP" ]
then	ZIP=bzip2
fi

if [ $PACKAGES -ne 0 ]
then	mkdir -p $PACKAGEDIR/All || true
	retrieve $PACKAGEDIR/All $PACKAGELIST packages/`uname -p`/$VERSION_PRETTY
fi

TMPDISKUSR=/dev/ram0
TMPDISKROOT=/dev/ram1

if [ ! -b $TMPDISKUSR -o ! $TMPDISKROOT ]
then	echo "$TMPDISKUSR or $TMPDISKROOT is not a block device.."
	exit 1
fi

if [ $TMPDISKUSR = $TMPDISKROOT ]
then
	echo "Temporary devices can't be equal."
	exit
fi

if [ $JAILMODE = 0 ]
then	echo " * Cleanup old files"
	umount $TMPDISKUSR || true
	umount $TMPDISKROOT || true
	umount $RELEASEMNTDIR/usr || true
	umount $RELEASEMNTDIR || true
fi

rm -rf $RELEASEDIR $RELEASEMNTDIR $IMG $ROOTIMAGE $CDFILES image* || true
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR $RELEASEMNTDIR 
mkdir -p $RELEASEPACKAGE

##########################################################################
echo " * Bootstrapping filesystem in $RELEASEDIR"
##########################################################################
CONFIGHEADER=$RELEASEDIR/usr/src/include/minix/sys_config.h

copy_local_packages

if [ "$COPY" -ne 1 ]
then
	echo "Retrieving latest minix repo from $REPO branch $GITBRANCH."
	srcdir=$RELEASEDIR/usr/src
	git clone -b $GITBRANCH $REPO $srcdir
	if [ "$REVTAG" ]
	then	echo "Doing checkout of $REVTAG."
		(cd $srcdir && git checkout $REVTAG )
	else	REVTAG=`(cd $srcdir && git rev-parse --short HEAD)`
		echo "Retrieved repository head in $srcdir is $REVTAG."
	fi
	if [ $MINIMAL -ne 0 ]
	then	rm -r $srcdir/.git
	fi
	echo "
/* Added by release script  */
#ifndef _VCS_REVISION
#define _VCS_REVISION \"$REVTAG\"
#endif" >>$CONFIGHEADER
	DATE=`date +%Y%m%d`
	# output image name
	if [ "$USB" -ne 0 ]; then
		IMG=${IMG_BASE}_${DATE}_${REVTAG}.img
	else
		IMG=${IMG_BASE}_${DATE}_${REVTAG}.iso
	fi
else
	echo "First cleaning current sourcedir.."
	( cd .. && make cleandir >/dev/null )
	echo "Copying contents from current src dir."
	srcdir=/usr/$SRC
	( cd $srcdir && tar --exclude .svn -cf - .  ) | ( cd $RELEASEDIR/usr && mkdir $SRC && cd $SRC && tar xf - )
	echo "Copying done."
	REVTAG=copy
	REVISION=unknown
	IMG=${IMG_BASE}_copy.iso
fi

# Make sure the CD knows it's a CD, unless it's not
if [ "$USB" -eq 0 ]
then	date >$RELEASEDIR/CD
fi

rm -f $RELEASEDIR/usr/$SRC/releasetools/revision

for p in $PREINSTALLED_PACKAGES
do	echo " * Pre-installing: $p from $PKG_ADD_URL"
    $PKG_ADD -f -P $RELEASEDIR $PKG_ADD_URL/$p
done

if [ "$CUSTOM_PACKAGES" ]
then	echo $PKG_ADD_URL >$RELEASEDIR/usr/pkg/etc/pkgin/repositories.conf
fi

echo " * Resetting timestamps"
find $RELEASEDIR | xargs touch

##########################################################################
echo " * Build"
##########################################################################

cd $RELEASEDIR/usr/src
make distribution DESTDIR=$RELEASEDIR CHECKFLIST=no
make -C releasetools do-hdboot DESTDIR=$RELEASEDIR MKINSTALLBOOT=yes
cp $RELEASEDIR/usr/mdec/boot_monitor $RELEASEDIR
cp $RELEASEDIR/boot/minix_latest/* $RELEASEDIR/boot/minix_default/

if [ $MAKEMAP -ne 0 ]; then
        find . -type f -perm 755 | xargs nm -n 2> /dev/null > symbols.txt
fi
make cleandir

cd -

echo " * build done"

##########################################################################
echo " * Removing bootstrap files"
##########################################################################
# The build process leaves some file in $SRC as bin.
chown -R root $RELEASEDIR/usr/src*
cp issue.install $RELEASEDIR/etc/issue

echo $version_pretty, SVN revision $REVISION, generated `date` >$RELEASEDIR/etc/version
rm -rf $RELEASEDIR/tmp/*

if [ $MINIMAL -ne 0 ]
then
	if [ "$MAKEMAP" -ne 0 ]
	then
		echo " * Copying symbol map to ${IMG}-symbols.txt"
		cp $RELEASEDIR/usr/src/symbols.txt ${IMG}-symbols.txt
		$ZIP -f ${IMG}-symbols.txt
	fi

	echo " * Removing files to create minimal image"
	rm -rf $RELEASEDIR/usr/man/man*/* 	\
		$RELEASEDIR/usr/share/zoneinfo* $RELEASEDIR/usr/src
	mkdir -p $RELEASEDIR/usr/src/releasetools
fi

if [ $EXTRAS_INSTALL -ne 0 ] ; then
    echo " * Copying files from $EXTRAS_PATH"
    cp -Rv $EXTRAS_PATH/* $RELEASEDIR
fi

# If we are making a jail, all is done!
if [ $JAILMODE = 1 ]
then	echo "Created new minix install in $RELEASEDIR."
	echo "Enter it by typing: "
	echo "# chroot $RELEASEDIR /bin/sh"
	exit 0
fi

##########################################################################
echo " * Counting files"
##########################################################################
extrakb=`du -ks $RELEASEDIR/usr/install | awk '{ print $1 }'`
find $RELEASEDIR/usr | fgrep -v /install/ | wc -l >$RELEASEDIR/.usrfiles
find $RELEASEDIR -print -path $RELEASEDIR/usr -prune | wc -l >$RELEASEDIR/.rootfiles

fstab_marker="# Poor man's File System Table."
echo " * Writing fstab"
if [ "$USB" -ne 0 ]
then
	echo \
"$fstab_marker
root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
" > $RELEASEDIR/etc/fstab
elif [ "$HDEMU" -ne 0 ]
then
	echo \
"$fstab_marker
root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
usr_roflag=\"-r\"" > $RELEASEDIR/etc/fstab
fi

##########################################################################
echo " * Mounting $TMPDISKROOT as $RELEASEMNTDIR"
##########################################################################
fitfs $RELEASEDIR $TMPDISKROOT 64 256 "$ROOTMB"
ROOTBLOCKS=$blocks
ROOTSECTS="`expr $blocks \* \( $BS / 512 \)`"
mount $TMPDISKROOT $RELEASEMNTDIR || exit

echo " * Mounting $TMPDISKUSR as $RELEASEMNTDIR/usr"
fitfs $RELEASEDIR/usr $TMPDISKUSR 0 0 "$USRMB"
USRBLOCKS=$blocks
USRSECTS="`expr $blocks \* \( $BS / 512 \)`"
mkdir -m 755 $RELEASEMNTDIR/usr
mount $TMPDISKUSR $RELEASEMNTDIR/usr || exit

##########################################################################
echo " * Copying files from staging to image"
##########################################################################
synctree -f $RELEASEDIR $RELEASEMNTDIR > /dev/null || true
expr `df -k $TMPDISKUSR | tail -1 | awk '{ print $4 }'` - $extrakb >$RELEASEMNTDIR/.usrkb

echo " * Unmounting $TMPDISKUSR from $RELEASEMNTDIR/usr"
umount $TMPDISKUSR || exit

echo " * Making image bootable"
if [ "$USB" -ne 0 ]
then
	usb_root_changes
elif [ "$HDEMU" -ne 0 ]
then
	hdemu_root_changes
else
	cd_root_changes
fi

echo " * Unmounting $TMPDISKROOT from $RELEASEMNTDIR"
umount $TMPDISKROOT || exit
rm -r $RELEASEMNTDIR

##########################################################################
echo " * Generating image files"
##########################################################################
dd if=$TMPDISKROOT of=$ROOTIMAGE bs=$BS count=$ROOTBLOCKS
cp release/cd/* $CDFILES || true
echo "This is Minix version $version_pretty prepared `date`." >$CDFILES/VERSION.TXT

boottype=-n
bootimage=$IMAGE
if [ "$HDEMU" -ne 0 ]; then
	make_hdimage
	boottype='-h'
	bootimage=hdimage
fi

if [ "$USB" -ne 0 ]; then
	mv $bootimage $IMG
else
	cp $RELEASEDIR/usr/mdec/boot_monitor $CDFILES/boot
	cp -rf $RELEASEDIR/boot/minix_latest/* $CDFILES/
	gzip -d $CDFILES/*gz
	writeisofs -s0x0 -l MINIX -B $bootimage $boottype $CDFILES $IMG || exit 1

	if [ "$HDEMU" -eq 0 ]
	then
		echo "Appending Minix root and usr filesystem"
		# Pad ISO out to cylinder boundary
		isobytes=`stat -f %z $IMG`
		isosects=`expr $isobytes / 512`
		isopad=`expr $secs - '(' $isosects % $secs ')'`
		dd if=/dev/zero count=$isopad >>$IMG
		# number of sectors
		isosects=`expr $isosects + $isopad`
		( cat $IMG $ROOTIMAGE ;
			dd if=$TMPDISKUSR bs=$BS count=$USRBLOCKS ) >m
		mv m $IMG
		# Make CD partition table
		installboot_nbsd -m $IMG /usr/mdec/mbr
		# Make sure there is no hole..! Otherwise the ISO format is
		# unreadable.
		partition -m $IMG 0 81:$isosects 81:$ROOTSECTS 81:$USRSECTS
	fi
fi

# Clean up: RELEASEDIR no longer needed
rm -r $RELEASEDIR

echo "${ZIP}ping $IMG"
$ZIP -f $IMG

if [ "$FILENAMEOUT" ]
then	echo "$IMG" >$FILENAMEOUT
fi

##########################################################################
echo " * Freeing up memory used by ramdisks"
##########################################################################
ramdisk 1 $TMPDISKROOT
ramdisk 1 $TMPDISKUSR
