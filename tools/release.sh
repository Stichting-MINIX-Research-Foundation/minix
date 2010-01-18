#!/bin/sh

set -e

PATH=$PATH:/usr/local/bin

XBIN=usr/xbin
SRC=src

# size of /tmp during build
TMPKB=32000

PACKAGEDIR=/usr/bigports/Packages
PACKAGESOURCEDIR=/usr/bigports/Sources
# List of packages included on installation media
PACKAGELIST=packages.install
# List of package source included on installation media
PACKAGESOURCELIST=package_sources.install
secs=`expr 32 '*' 64`
export SHELL=/bin/sh

# SVN trunk repo
TRUNK=https://gforge.cs.vu.nl/svn/minix/trunk

make_hdimage()
{
	dd if=$TMPDISK1 of=usrimage bs=$BS count=$USRBLOCKS

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
	url=http://www.minix3.org/$3
BIGPORTS=bigports
	(	
		cd $dir || exit 1
		echo  " * Updating $dir
   from $url
   with $list"
   		files=`awk <$list '{ print "'$url'/" $1 ".tar.bz2" }'`
		wget -c $url/List $files || true
	)
}

hdemu_root_changes()
{
	$RELEASEDIR/usr/bin/installboot -d $TMPDISK3 \
		$RELEASEDIR/usr/mdec/bootblock boot/boot
	echo \
'bootcd=2
disable=inet
bios_wini=yes
bios_remap_first=1
ramimagedev=c0d7p0s0
bootbig(1, Regular MINIX 3) { image=/boot/image_big; boot }
main() { trap 10000 boot ; menu; }
save'	| $RELEASEDIR/usr/bin/edparams $TMPDISK3

	echo \
'root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
usr_roflag="-r"' > $RELEASEDIR/etc/fstab
}

usb_root_changes()
{
	$RELEASEDIR/usr/bin/installboot -d $TMPDISK3 \
		$RELEASEDIR/usr/mdec/bootblock boot/boot
	echo \
'bios_wini=yes
bios_remap_first=1
rootdev=c0d7p0s0
save'	| $RELEASEDIR/usr/bin/edparams $TMPDISK3

	echo \
'root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
' > $RELEASEDIR/etc/fstab
}

RELEASEDIR=/usr/r
RELEASEPACKAGE=${RELEASEDIR}/usr/install/packages
RELEASEPACKAGESOURCES=${RELEASEDIR}/usr/install/package-sources
IMAGE=../boot/boot
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

FILENAMEOUT=""

while getopts "s:pchu?r:f:" c
do
	case "$c" in
	\?)
		echo "Usage: $0 [-p] [-c] [-h] [-r <tag>] [-u] [-f <filename>] [-s <username>]" >&2
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
	esac
done

if [ ! "$USRMB" ]
then	USRMB=600
fi

if [ $PACKAGES -ne 0 ]
then	mkdir -p $PACKAGEDIR || true
	mkdir -p $PACKAGESOURCEDIR || true
	rm -f $PACKAGEDIR/List
	retrieve $PACKAGEDIR $PACKAGELIST packages/`uname -p`/`uname -r`.`uname -v`
	retrieve $PACKAGESOURCEDIR $PACKAGESOURCELIST software
fi

echo $USRMB MB

USRKB=$(($USRMB*1024))
USRBLOCKS=$(($USRMB * 1024 * 1024 / $BS))
USRSECTS=$(($USRMB * 1024 * 2))
ROOTKB=8192
ROOTSECTS=$(($ROOTKB * 2))
ROOTBLOCKS=$(($ROOTKB * 1024 / $BS))

if [ "$COPY" -ne 1 ]
then
	echo "Note: this script wants to do svn operations."
fi

TMPDISK1=/dev/ram0
TMPDISK2=/dev/ram1
TMPDISK3=/dev/ram2

if [ ! -b $TMPDISK1 -o ! -b $TMPDISK2 -o ! $TMPDISK3 ]
then	echo "$TMPDISK1, $TMPDISK2 or $TMPDISK3 is not a block device.."
	exit 1
fi

umount $TMPDISK1 || true
umount $TMPDISK2 || true
umount $TMPDISK3 || true

ramdisk $USRKB $TMPDISK1
ramdisk $TMPKB $TMPDISK2
ramdisk $ROOTKB $TMPDISK3

if [ $TMPDISK1 = $TMPDISK2  -o $TMPDISK1 = $TMPDISK3 -o $TMPDISK2 = $TMPDISK3 ]
then
	echo "Temporary devices can't be equal."
	exit
fi

echo " * Cleanup old files"
rm -rf $RELEASEDIR $IMG $ROOTIMAGE $CDFILES image*
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR
mkfs -i 2000 -B $BS -b $ROOTBLOCKS $TMPDISK3 || exit
mkfs -B 1024 -b $TMPKB  $TMPDISK2 || exit
echo " * mounting $TMPDISK3 as $RELEASEDIR"
mount $TMPDISK3 $RELEASEDIR || exit
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp
mount $TMPDISK2 $RELEASEDIR/tmp

mkfs -B $BS -i 30000 -b $USRBLOCKS $TMPDISK1 || exit
echo " * Mounting $TMPDISK1 as $RELEASEDIR/usr"
mount $TMPDISK1 $RELEASEDIR/usr || exit
mkdir -p $RELEASEDIR/tmp
mkdir -p $RELEASEDIR/usr/tmp
mkdir -p $RELEASEDIR/$XBIN
mkdir -p $RELEASEDIR/usr/bin
mkdir -p $RELEASEDIR/bin
mkdir -p $RELEASEPACKAGE
mkdir -p $RELEASEPACKAGESOURCES

echo " * Transfering bootstrap dirs to $RELEASEDIR"
cp -p /bin/* /usr/bin/* $RELEASEDIR/$XBIN
cp -rp /usr/lib $RELEASEDIR/usr
cp -rp /bin/bigsh /bin/sh /bin/echo $RELEASEDIR/bin
cp -rp /usr/bin/make /usr/bin/install /usr/bin/yacc /usr/bin/flex $RELEASEDIR/usr/bin

if [ -d $PACKAGEDIR -a -d $PACKAGESOURCEDIR -a -f $PACKAGELIST -a -f $PACKAGESOURCELIST -a $PACKAGES -ne 0 ]
then	echo " * Transfering $PACKAGEDIR to $RELEASEPACKAGE"
	: >$RELEASEPACKAGE/List
        for p in `cat $PACKAGELIST`
        do	if [ -f $PACKAGEDIR/$p.tar.bz2 ]
               then
		  cp $PACKAGEDIR/$p.tar.bz2 $RELEASEPACKAGE/
		  grep "^$p|" $PACKAGEDIR/List >>$RELEASEPACKAGE/List || echo "$p not found in List"
               else
                  echo "Can't copy $PACKAGEDIR/$p.tar.bz2. Missing."
               fi
        done
	
	echo " * Transfering $PACKAGESOURCEDIR to $RELEASEPACKAGESOURCES"
        for p in `cat $PACKAGESOURCELIST`
        do
               if [ -f $PACKAGESOURCEDIR/$p.tar.bz2 ]
               then
	          cp $PACKAGESOURCEDIR/$p.tar.bz2 $RELEASEPACKAGESOURCES/
               else
                  echo "Can't copy $PACKAGESOURCEDIR/$p.tar.bz2. Missing."
               fi
        done
fi

# Make sure compilers and libraries are bin-owned
chown -R bin $RELEASEDIR/usr/lib
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
chown -R bin $RELEASEDIR/usr/$SRC 
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
echo " * Chroot build"
cp chrootmake.sh $RELEASEDIR/usr/$SRC/tools/chrootmake.sh
chroot $RELEASEDIR "PATH=/$XBIN sh -x /usr/$SRC/tools/chrootmake.sh" || exit 1
# Copy built images for cd booting
cp $RELEASEDIR/boot/image_big image
echo " * Chroot build done"
echo " * Removing bootstrap files"
rm -rf $RELEASEDIR/$XBIN
# The build process leaves some file in $SRC as root.
chown -R bin $RELEASEDIR/usr/src*
cp issue.install $RELEASEDIR/etc/issue

if [ "$USB" -ne 0 ]
then
	usb_root_changes
elif [ "$HDEMU" -ne 0 ]
then
	hdemu_root_changes
fi

echo $version_pretty, SVN revision $REVISION, generated `date` >$RELEASEDIR/etc/version
echo " * Counting files"
extrakb=`du -s $RELEASEDIR/usr/install | awk '{ print $1 }'`
expr `df $TMPDISK1 | tail -1 | awk '{ print $4 }'` - $extrakb >$RELEASEDIR/.usrkb
find $RELEASEDIR/usr | fgrep -v /install/ | wc -l >$RELEASEDIR/.usrfiles
find $RELEASEDIR -xdev | wc -l >$RELEASEDIR/.rootfiles
echo " * Zeroing remainder of temporary areas"
df $TMPDISK1
df $TMPDISK3
cp /dev/zero $RELEASEDIR/usr/.x 2>/dev/null || true
rm $RELEASEDIR/usr/.x
cp /dev/zero $RELEASEDIR/.x 2>/dev/null || true
rm $RELEASEDIR/.x

umount $TMPDISK1 || exit
umount $TMPDISK2 || exit
umount $TMPDISK3 || exit

# Boot monitor variables for boot CD
edparams $TMPDISK3 'unset bootopts;
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

(cd ../boot && make)
dd if=$TMPDISK3 of=$ROOTIMAGE bs=$BS count=$ROOTBLOCKS
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
	writeisofs -s0x1000 -l MINIX -b $bootimage $boottype $CDFILES $IMG || exit 1

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
			dd if=$TMPDISK1 bs=$BS count=$USRBLOCKS ) >m
		mv m $IMG
		# Make CD partition table
		installboot -m $IMG /usr/mdec/masterboot
		# Make sure there is no hole..! Otherwise the ISO format is
		# unreadable.
		partition -m $IMG 0 81:$isosects 81:$ROOTSECTS 81:$USRSECTS
	fi
fi

if [ "$FILENAMEOUT" ]
then	echo "$IMG" >$FILENAMEOUT
fi
