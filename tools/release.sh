#!/bin/sh

set -e

XBIN=usr/xbin
SRC=src

PACKAGEDIR=/usr/bigports/Packages
PACKAGESOURCEDIR=/usr/bigports/Sources
secs=`expr 32 '*' 64`
export SHELL=/bin/sh

make_hdimage()
{
	dd if=$TMPDISK of=usrimage bs=$BS count=$USRBLOCKS

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
bootbig(1, MINIX 3) { image=/boot/image_big; boot }
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
IMAGE=cdfdimage
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

USRMB=580

USRBLOCKS="`expr $USRMB \* 1024 \* 1024 / $BS`"
USRSECTS="`expr $USRMB \* 1024 \* 2`"
ROOTKB=4096
ROOTSECTS="`expr $ROOTKB \* 2`"
ROOTBLOCKS="`expr $ROOTKB \* 1024 / $BS`"

if [ "$COPY" -ne 1 ]
then
	echo "Note: this script wants to do svn operations."
fi

TD1=.td1
TD2=.td2
TD3=.td3


if [ -f $TD1 ]
then    TMPDISK="`cat $TD1`"
	echo " * Warning: I'm going to overwrite $TMPDISK!"
else
        echo "Temporary (sub)partition to use to make the /usr FS image? "
        echo "I need $USRMB MB. It will be mkfsed!"
        echo -n "Device: /dev/"
        read dev || exit 1
        TMPDISK=/dev/$dev
fi

if [ -b $TMPDISK ]
then :
else	echo "$TMPDISK is not a block device.."
	exit 1
fi

echo $TMPDISK >$TD1

if [ -f $TD2 ]
then    TMPDISK2="`cat $TD2`"
	echo " * Warning: I'm going to overwrite $TMPDISK2!"
else
        echo "Temporary (sub)partition to use for /tmp? "
        echo "It will be mkfsed!"
        echo -n "Device: /dev/"
        read dev || exit 1
        TMPDISK2=/dev/$dev
fi

if [ -b $TMPDISK2 ]
then :
else	echo "$TMPDISK2 is not a block device.."
	exit 1
fi

echo $TMPDISK2 >$TD2

if [ -f $TD3 ]
then    TMPDISK3="`cat $TD3`"
	echo " * Warning: I'm going to overwrite $TMPDISK3!"
else
        echo "It has to be at least $ROOTKB KB."
        echo ""
        echo "Temporary (sub)partition to use to make the root FS image? "
        echo "It will be mkfsed!"
        echo -n "Device: /dev/"
        read dev || exit 1
        TMPDISK3=/dev/$dev
fi

if [ -b $TMPDISK3 ]
then :
else	echo "$TMPDISK3 is not a block device.."
	exit 1
fi

echo $TMPDISK3 >$TD3

umount $TMPDISK || true
umount $TMPDISK2 || true
umount $TMPDISK3 || true

if [ $TMPDISK = $TMPDISK2  -o $TMPDISK = $TMPDISK3 -o $TMPDISK2 = $TMPDISK3 ]
then
	echo "Temporary devices can't be equal."
	exit
fi

echo " * Cleanup old files"
rm -rf $RELEASEDIR $IMG $IMAGE $ROOTIMAGE $CDFILES image*
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR
mkfs -B $BS -b $ROOTBLOCKS $TMPDISK3 || exit
mkfs $TMPDISK2 || exit
echo " * mounting $TMPDISK3 as $RELEASEDIR"
mount $TMPDISK3 $RELEASEDIR || exit
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp
mount $TMPDISK2 $RELEASEDIR/tmp

echo making /usr
mkfs -B $BS -b $USRBLOCKS $TMPDISK || exit
echo " * Mounting $TMPDISK as $RELEASEDIR/usr"
mount $TMPDISK $RELEASEDIR/usr || exit
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

if [ -d $PACKAGEDIR -a -d $PACKAGESOURCEDIR -a $PACKAGES -ne 0 ]
then	echo " * Indexing packages"
	bintotal=0
	( cd $PACKAGEDIR
	  for p in *.tar.bz2
	  do	echo $p >&2
		p="`echo $p | sed 's/.tar.bz2//'`"
		descr="../$p/.descr"
		if [ -f "$descr" ]
		then	echo "$p|`cat $descr`"
		fi
	  done >List
	)
	for d in $PACKAGEDIR $PACKAGESOURCEDIR
	do	echo Counting size of $d
		f=$d/SizeMB
		if [ ! -f $f ]
		then
			b="`bzip2 -dc $d/*.bz2 | wc -c`"
			echo "`expr 1 + $b / 1024 / 1024`" >$f
		fi
		echo "`cat $f` MB."
	done
	echo " * Transfering $PACKAGEDIR to $RELEASEPACKAGE"
	cp $PACKAGEDIR/* $RELEASEPACKAGE/
	echo " * Transfering $PACKAGESOURCEDIR to $RELEASEPACKAGESOURCES"
	cp $PACKAGESOURCEDIR/* $RELEASEPACKAGESOURCES/ || true

fi

# Make sure compilers and libraries are bin-owned
chown -R bin $RELEASEDIR/usr/lib
chmod -R u+w $RELEASEDIR/usr/lib

if [ "$COPY" -ne 1 ]
then
	echo " * Doing new svn export"
	BRANCH=r3.1.3
	REPO=https://gforge.cs.vu.nl/svn/minix/branches/$BRANCH
	REVISION="`svn info $USERNAME $SVNREV $REPO | grep '^Revision: ' | awk '{ print $2 }'`"
	echo "Doing export of revision $REVISION from $REPO."
	( cd $RELEASEDIR/usr && svn $USERNAME export -r$REVISION $REPO && mv $BRANCH $SRC )
	REVTAG=r$REVISION
	echo "

/* Added by release script  */
#ifndef _SVN_REVISION
#define _SVN_REVISION \"$REVISION\"
#endif" >>$RELEASEDIR/usr/src/include/minix/sys_config.h

else
	( cd .. && make depend && make clean )
	srcdir=/usr/$SRC
	( cd $srcdir && tar cf - . ) | ( cd $RELEASEDIR/usr && mkdir $SRC && cd $SRC && tar xf - )
	REVTAG=copy
fi

if [ "$USB" -ne 0 ]; then
	IMG=${IMG_BASE}_${REVTAG}.img
else
	IMG=${IMG_BASE}_${REVTAG}.iso
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

echo $version_pretty, SVN revision $SVNREV, generated `date` >$RELEASEDIR/etc/version
echo " * Counting files"
extrakb=`du -s $RELEASEDIR/usr/install | awk '{ print $1 }'`
expr `df $TMPDISK | tail -1 | awk '{ print $4 }'` - $extrakb >$RELEASEDIR/.usrkb
find $RELEASEDIR/usr | fgrep -v /install/ | wc -l >$RELEASEDIR/.usrfiles
find $RELEASEDIR -xdev | wc -l >$RELEASEDIR/.rootfiles
echo " * Zeroing remainder of temporary areas"
df $TMPDISK
df $TMPDISK3
cp /dev/zero $RELEASEDIR/usr/.x 2>/dev/null || true
rm $RELEASEDIR/usr/.x
cp /dev/zero $RELEASEDIR/.x 2>/dev/null || true
rm $RELEASEDIR/.x

umount $TMPDISK || exit
umount $TMPDISK2 || exit
umount $TMPDISK3 || exit

(cd ../boot && make)
dd if=$TMPDISK3 of=$ROOTIMAGE bs=$BS count=$ROOTBLOCKS
sh mkboot cdfdboot $TMPDISK3
cp $IMAGE $CDFILES/bootflop.img
cp release/cd/* $CDFILES || true
echo "This is Minix version $version_pretty prepared `date`." >$CDFILES/VERSION.TXT

h_opt=
bootimage=$IMAGE
if [ "$HDEMU" -ne 0 ]; then
	make_hdimage
	h_opt='-h'
	bootimage=hdimage
fi

if [ "$USB" -ne 0 ]; then
	mv $bootimage $IMG
else
	writeisofs -l MINIX -b $bootimage $h_opt $CDFILES $IMG || exit 1

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
			dd if=$TMPDISK bs=$BS count=$USRBLOCKS ) >m
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
