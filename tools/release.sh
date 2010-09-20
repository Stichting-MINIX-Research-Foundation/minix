#!/bin/sh

set -e

XBIN=usr/xbin
SRC=src

PACKAGEDIR=/usr/pkgsrc/packages/`uname -r`/`uname -m`
# List of packages included on installation media
PACKAGELIST=packages.install
secs=`expr 32 '*' 64`
export SHELL=/bin/sh

# Packages we have to pre-install, and url to use
PREINSTALLED_PACKAGES=pkgin-0.3.3.1nb1
PACKAGEURL=ftp://ftp.minix3.org/pub/minix/packages/`uname -r`/`uname -m`/All/

RELEASERC=$HOME/.releaserc

if [ -f $RELEASERC ]
then	. $RELEASERC
fi

set -- $* $RELOPTS

# SVN trunk repo
TRUNK=https://gforge.cs.vu.nl/svn/minix/trunk

make_hdimage()
{
	dd if=$TMPDISKUSR of=usrimage bs=$BS count=$USRBLOCKS

	rootsize=`stat -size rootimage`
	usrsize=`stat -size usrimage`

	rootsects=`expr $rootsize / 512`
	usrsects=`expr $usrsize / 512`

	# installboot -m needs at least 1KB 
	dd < /dev/zero >tmpimage count=2
	partition -fm tmpimage 2 81:$rootsects* 0:0 81:$usrsects
	installboot -m tmpimage /usr/mdec/masterboot
	dd < tmpimage > subpart count=1

	primsects=`expr 1 + $rootsects + $usrsects`
	cyl=`expr '(' $primsects ')' / $secs + 1`
	padsects=`expr $cyl \* $secs - 1 - $primsects`

	{ dd < /dev/zero count=1
		cat subpart
		cat rootimage
		cat usrimage
		dd < /dev/zero count=$padsects
	} > hdimage
	partition -m hdimage 81:`expr $primsects + $padsects`*
	installboot -m hdimage /usr/mdec/masterboot
}

retrieve()
{
	dir=$1
	list=`pwd`/$2
	url=${PACKAGEURL}
	(	
		cd $dir || exit 1
		echo  " * Updating $dir
   from $url
   with $list"
		files=`awk <$list '{ print "'$url'/" $1 ".tgz" }'`
		fetch -r $files || true
	)
}

cd_root_changes()
{
	edparams $TMPDISKROOT 'unset bootopts;
unset servers;
unset rootdev;
unset leader;
unset image;
disable=inet;
bootcd=1;
cdproberoot=1;
ata_id_timeout=2;
bootbig(1, Regular MINIX 3) { unset image; boot }
leader() { echo \n--- Welcome to MINIX 3. This is the boot monitor. ---\n\nChoose an option from the menu or press ESC if you need to do anything special.\nOtherwise I will boot with my defaults in 10 seconds.\n\n }; main(){trap 10000 boot; menu; };
save' 
}

hdemu_root_changes()
{
	$RELEASEDIR/usr/bin/installboot -d $TMPDISKROOT \
		$RELEASEDIR/usr/mdec/bootblock boot/boot
	echo \
'bootcd=2
disable=inet
bios_wini=yes
bios_remap_first=1
ramimagedev=c0d7p0s0
bootbig(1, Regular MINIX 3) { image=/boot/image_big; boot }
main() { trap 10000 boot ; menu; }
save'	| $RELEASEDIR/usr/bin/edparams $TMPDISKROOT
}

usb_root_changes()
{
	$RELEASEDIR/usr/bin/installboot -d $TMPDISKROOT \
		$RELEASEDIR/usr/mdec/bootblock boot/boot
	echo \
'bios_wini=yes
bios_remap_first=1
rootdev=c0d7p0s0
bootbig(1, Regular MINIX 3) { image=/boot/image_big; boot }
leader() { echo \n--- Welcome to MINIX 3. This is the boot monitor. ---\n\nChoose an option from the menu or press ESC if you need to do anything special.\nOtherwise I will boot with my defaults in 10 seconds.\n\n }; main(){trap 10000 boot; menu; };
save'	| $RELEASEDIR/usr/bin/edparams $TMPDISKROOT
}

fitfs()
{
	path="$1"
	ramdisk="$2"
	extra_inodes="$3"
	extra_zones="$4"
	mbsdefault="$5"

	# Determine number of inodes
	inodes=`find $path | egrep -v ^$path/usr | wc -l`
	inodes="`expr $inodes + $extra_inodes`"

	# Determine number of data zones
	zonekbs=`du -Fs $path | cut -d'	' -f1`
	zonekbsignore=0
	[ ! -d $path/usr ] || zonekbsignore=`du -Fs $path/usr | cut -d"	" -f1`
	zones="`expr \( $zonekbs - $zonekbsignore \) / \( $BS / 1024 \) + $extra_zones`"

	# Determine file system size
	BSBITS="`expr $BS \* 8`"
	imap_blocks="`expr \( $inodes + $BSBITS - 1 \) / $BSBITS`"
	inode_blocks="`expr \( $inodes \* 64 + $BS - 1 \) / $BS`"
	zmap_blocks="`expr \( $zones + $BSBITS - 1 \) / $BSBITS`"
	blocks="`expr 1 + 1 + $imap_blocks + $zmap_blocks + $inode_blocks + $zones`"
	kbs="`expr $blocks \* \( $BS / 1024 \)`"

	# Apply default if higher
	if [ -n "$mbsdefault" ]
	then
		kbsdefault="`expr $mbsdefault \* 1024`"
		if [ "$kbs" -lt "$kbsdefault" ]
		then kbs=$kbsdefault
		else echo "warning: ${mbsdefault}mb is too small, using ${kbs}kb"
		fi
	fi

	# Create a filesystem on the target ramdisk
	ramdisk $kbs $ramdisk
	mkfs.mfs -B $BS -i $inodes $ramdisk
}

RELEASEDIR=/usr/r-staging
RELEASEMNTDIR=/usr/r
RELEASEPACKAGE=${RELEASEDIR}/usr/install/packages

IMAGE=../boot/cdbootblock
ROOTIMAGE=rootimage
CDFILES=/usr/tmp/cdreleasefiles
sh tell_config OS_RELEASE . OS_VERSION >/tmp/rel.$$
version_pretty=`sed 's/["      ]//g;/^$/d' </tmp/rel.$$`
version=`sed 's/["      ]//g;/^$/d' </tmp/rel.$$ | tr . _`
IMG_BASE=minix${version}_ide
BS=4096

HDEMU=0
COPY=0
SVNREV=""
REVTAG=""
PACKAGES=1
MINIMAL=0
MAKEMAP=0

FILENAMEOUT=""

while getopts "s:pmMchu?r:f:" c
do
	case "$c" in
	\?)
		echo "Usage: $0 [-p] [-c] [-h] [-m] [-M] [-r <tag>] [-u] [-f <filename>] [-s <username>]" >&2
		exit 1
	;;
	h)
		echo " * Making HD image"
		IMG_BASE=minix${version}_bios
		HDEMU=1
		;;
	c)
		echo " * Copying, not SVN"
		COPY=1
		;;
	p)
		PACKAGES=0
		;;
	r)	
		SVNREV=-r$OPTARG
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
	esac
done

if [ ! "$ZIP" ]
then	ZIP=bzip2
fi

if [ $PACKAGES -ne 0 ]
then	mkdir -p $PACKAGEDIR/All || true
	retrieve $PACKAGEDIR/All $PACKAGELIST packages/`uname -p`/`uname -r`
fi

if [ "$COPY" -ne 1 ]
then
	echo "Note: this script wants to do svn operations."
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

echo " * Cleanup old files"
umount $TMPDISKUSR || true
umount $TMPDISKROOT || true
umount $RELEASEMNTDIR/usr || true
umount $RELEASEMNTDIR || true

rm -rf $RELEASEDIR $RELEASEMNTDIR $IMG $ROOTIMAGE $CDFILES image*
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR $RELEASEMNTDIR 
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp

mkdir -p $RELEASEDIR/tmp
mkdir -p $RELEASEDIR/usr/tmp
mkdir -p $RELEASEDIR/$XBIN
mkdir -p $RELEASEDIR/usr/bin
mkdir -p $RELEASEDIR/bin
mkdir -p $RELEASEPACKAGE

echo " * Transfering bootstrap dirs to $RELEASEDIR"
cp -p /bin/* /usr/bin/* /sbin/* $RELEASEDIR/$XBIN
cp -rp /usr/lib $RELEASEDIR/usr
cp -rp /bin/sh /bin/echo $RELEASEDIR/bin
cp -rp /usr/bin/make /usr/bin/install /usr/bin/yacc /usr/bin/lex /usr/bin/asmconv $RELEASEDIR/usr/bin

if [ -d $PACKAGEDIR -a -f $PACKAGELIST -a $PACKAGES -ne 0 ]
then
	index=pkg_summary
	indexpath=$PACKAGEDIR/.index

	if [ ! -d $indexpath ]
	then	mkdir $indexpath
	fi
	if [ ! -d $indexpath ]
	then	echo "Couldn't create $indexpath."
		exit 1
	fi

	echo "" >$PACKAGEDIR/All/$index

        echo " * Transfering $PACKAGEDIR to $RELEASEPACKAGE"
        for p in `cat $PACKAGELIST`
        do	if [ -f $PACKAGEDIR/All/$p.tgz ]
               then
		  # Copy package and create package's index
		  (
		      cd $PACKAGEDIR/All
		      cp $p.tgz $RELEASEPACKAGE/

		      f=$p.tgz
		      indexname=$indexpath/$f.$index
		      pkg_info -X $f >$indexname

		      if [ ! -f $indexname ]
		      then	echo Missing $indexname.
			  exit 1
		      fi

		      if [ "`wc -l $indexname`" -lt 3 ]
		      then	$indexname is too short.
			  rm $indexname
			  exit 1
		      fi

		      cat $indexname >>$PACKAGEDIR/All/$index
		  )
               else
                  echo "Can't copy $PACKAGEDIR/$p.tgz. Missing."
               fi
        done

	bzip2 -f $PACKAGEDIR/All/$index
	cp $PACKAGEDIR/All/$index.bz2 $RELEASEPACKAGE/
fi

# Make sure compilers and libraries are root-owned
chown -R root $RELEASEDIR/usr/lib
chmod -R u+w $RELEASEDIR/usr/lib

if [ "$COPY" -ne 1 ]
then
	echo " * Doing new svn export"
	TOOLSREPO="`svn info | grep '^URL: ' | awk '{ print $2 }'`"
	REPO="`echo $TOOLSREPO | sed 's/.tools$//'`"
	BRANCHNAME="`echo $REPO | awk -F/ '{ print $NF }'`"
	REVISION="`svn info $USERNAME $SVNREV $REPO | grep '^Revision: ' | awk '{ print $2 }'`"
	echo "Doing export of revision $REVISION from $REPO."
	( cd $RELEASEDIR/usr && svn -q $USERNAME export -r$REVISION $REPO $SRC )
	if [ $BRANCHNAME = src ]
	then	REVTAG=r$REVISION
	else	REVTAG=branch-$BRANCHNAME-r$REVISION
	fi
	
	echo "

/* Added by release script  */
#ifndef _SVN_REVISION
#define _SVN_REVISION \"$REVTAG\"
#endif" >>$RELEASEDIR/usr/src/include/minix/sys_config.h

# output image name
if [ "$USB" -ne 0 ]; then
	IMG=${IMG_BASE}_${REVTAG}.img
else
	IMG=${IMG_BASE}_${REVTAG}.iso
fi

else
	( cd .. && make depend && make clean )
	srcdir=/usr/$SRC
	( cd $srcdir && tar cf - . ) | ( cd $RELEASEDIR/usr && mkdir $SRC && cd $SRC && tar xf - )
	REVTAG=copy
	REVISION=unknown
	IMG=${IMG_BASE}_copy.iso
fi

echo " * Fixups for owners and modes of dirs and files"
chown -R root $RELEASEDIR/usr/$SRC
chmod -R u+w $RELEASEDIR/usr/$SRC 
find $RELEASEDIR/usr/$SRC -type d | xargs chmod 755
find $RELEASEDIR/usr/$SRC -type f | xargs chmod 644
find $RELEASEDIR/usr/$SRC -name configure | xargs chmod 755
find $RELEASEDIR/usr/$SRC/commands -name build | xargs chmod 755
# Bug tracking system not for on cd
rm -rf $RELEASEDIR/usr/$SRC/doc/bugs

# Make sure the CD knows it's a CD, unless it's not
if [ "$USB" -eq 0 ]
then	date >$RELEASEDIR/CD
fi
echo " * Bootstrap /usr/share/mk files"
# Need /usr/share/mk in the new system to invoke make. Real ownerships
# and permissions will be set by its own src/share/mk/Makefile.
mkdir -p $RELEASEDIR/usr/share/mk
chmod 755 $RELEASEDIR/usr/share/mk
cp $RELEASEDIR/usr/src/share/mk/* $RELEASEDIR/usr/share/mk/
chown -R root $RELEASEDIR/usr/share/mk
cp chrootmake.sh $RELEASEDIR/usr/$SRC/tools/chrootmake.sh

echo " * Make hierarchy"
chroot $RELEASEDIR "PATH=/$XBIN sh -x /usr/$SRC/tools/chrootmake.sh etcfiles" || exit 1

for p in $PREINSTALLED_PACKAGES
do	echo " * Pre-installing: $p from $url"
    pkg_add -P $RELEASEDIR $PACKAGEURL/$p
done

echo " * Chroot build"
chroot $RELEASEDIR "PATH=/$XBIN MAKEMAP=$MAKEMAP sh -x /usr/$SRC/tools/chrootmake.sh" || exit 1
# Copy built images for cd booting
cp $RELEASEDIR/boot/image_big image
echo " * Chroot build done"
echo " * Removing bootstrap files"
rm -rf $RELEASEDIR/$XBIN
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
	rm -rf $RELEASEDIR/boot/image/* $RELEASEDIR/usr/man/man*/* 	\
		$RELEASEDIR/usr/share/zoneinfo* $RELEASEDIR/usr/src
	mkdir -p $RELEASEDIR/usr/src/tools
	ln $RELEASEDIR/boot/image_big $RELEASEDIR/boot/image/$version
fi

echo " * Counting files"
extrakb=`du -s $RELEASEDIR/usr/install | awk '{ print $1 }'`
find $RELEASEDIR/usr | fgrep -v /install/ | wc -l >$RELEASEDIR/.usrfiles
find $RELEASEDIR -print -path $RELEASEDIR/usr -prune | wc -l >$RELEASEDIR/.rootfiles

echo " * Writing fstab"
if [ "$USB" -ne 0 ]
then
	echo \
'root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
' > $RELEASEDIR/etc/fstab
elif [ "$HDEMU" -ne 0 ]
then
	echo \
'root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
usr_roflag="-r"' > $RELEASEDIR/etc/fstab
fi

echo " * Mounting $TMPDISKROOT as $RELEASEMNTDIR"
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

echo " * Copying files from staging to image"
synctree -f $RELEASEDIR $RELEASEMNTDIR > /dev/null || true
expr `df $TMPDISKUSR | tail -1 | awk '{ print $4 }'` - $extrakb >$RELEASEMNTDIR/.usrkb

echo " * Unmounting $TMPDISKUSR from $RELEASEMNTDIR/usr"
umount $TMPDISKUSR || exit
echo " * Unmounting $TMPDISKROOT from $RELEASEMNTDIR"
umount $TMPDISKROOT || exit
rm -r $RELEASEMNTDIR

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

# Clean up: RELEASEDIR no longer needed
rm -r $RELEASEDIR

(cd ../boot && make)
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
	cp ../boot/boot $CDFILES
	writeisofs -s0x0 -l MINIX -a boot -b $bootimage $boottype $CDFILES $IMG || exit 1

	if [ "$HDEMU" -eq 0 ]
	then
		echo "Appending Minix root and usr filesystem"
		# Pad ISO out to cylinder boundary
		isobytes=`stat -size $IMG`
		isosects=`expr $isobytes / 512`
		isopad=`expr $secs - '(' $isosects % $secs ')'`
		dd if=/dev/zero count=$isopad >>$IMG
		# number of sectors
		isosects=`expr $isosects + $isopad`
		( cat $IMG $ROOTIMAGE ;
			dd if=$TMPDISKUSR bs=$BS count=$USRBLOCKS ) >m
		mv m $IMG
		# Make CD partition table
		installboot -m $IMG /usr/mdec/masterboot
		# Make sure there is no hole..! Otherwise the ISO format is
		# unreadable.
		partition -m $IMG 0 81:$isosects 81:$ROOTSECTS 81:$USRSECTS
	fi
fi
echo "${ZIP}ping $IMG"
$ZIP -f $IMG

if [ "$FILENAMEOUT" ]
then	echo "$IMG" >$FILENAMEOUT
fi

echo " * Freeing up memory used by ramdisks"
ramdisk 1 $TMPDISKROOT
ramdisk 1 $TMPDISKUSR
