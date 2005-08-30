#!/bin/sh

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
	cyl=`expr '(' $primsects ')' / 32 / 64 + 1`
	padsects=`expr $cyl \* 32 \* 64 - 1 - $primsects`

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
	$RELEASEDIR/usr/bin/installboot -d $RAM \
		$RELEASEDIR/usr/mdec/bootblock boot/boot
	echo \
'label=BIOS
bootcd=2
disable=inet
bios_remap_first=1
ramimagedev=c0d7p0s0
save'	| $RELEASEDIR/usr/bin/edparams $RAM

	echo \
'root=/dev/c0d7p0s0
usr=/dev/c0d7p0s2
usr_roflag="-r"' > $RELEASEDIR/etc/fstab
}

HDEMU=1

COPYITEMS="usr/bin bin usr/lib"
RELEASEDIR=/usr/r/release
IMAGE=cdfdimage
ROOTIMAGE=rootimage
CDFILES=/usr/tmp/cdreleasefiles
ISO=minix.iso
ISOGZ=minix.iso.gz
RAM=/dev/ram
BS=4096
USRMB=150
USRBLOCKS="`expr $USRMB \* 1024 \* 1024 / $BS`"
ROOTMB=2
ROOTBLOCKS="`expr $ROOTMB \* 1024 \* 1024 / $BS`"

HDEMU=0
COPY=0

while getopts "ch?" c
do
	case "$c" in
	\?)
		echo "Usage: $0 [-c] [-h]" >&2
		exit 1
	;;
	h)
		HDEMU=1
		;;
	c)
		COPY=1
		;;
	esac
done

if [ "$COPY" -ne 1 ]
then
	echo "Note: this script wants to do cvs operations, so it's necessary"
	echo "to have \$CVSROOT set and cvs login done."
	echo ""
fi

echo "Warning: I'm going to mkfs $RAM! It has to be at least $ROOTMB MB."
echo ""
echo "Temporary (sub)partition to use to make the /usr FS image? "
echo "I need $USRMB MB. It will be mkfsed!"
echo -n "Device: /dev/"
read dev || exit 1
TMPDISK=/dev/$dev

if [ -b $TMPDISK ]
then :
else	echo "$TMPDISK is not a block device.."
	exit 1
fi

echo "Temporary (sub)partition to use for /tmp? "
echo "It will be mkfsed!"
echo -n "Device: /dev/"
read dev || exit 1
TMPDISK2=/dev/$dev

if [ -b $TMPDISK2 ]
then :
else	echo "$TMPDISK2 is not a block device.."
	exit 1
fi

umount $TMPDISK
umount $TMPDISK2
umount $RAM

echo " * Cleanup old files"
rm -rf $RELEASEDIR $ISO $IMAGE $ROOTIMAGE $ISOGZ $CDFILES
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR
echo " * Zeroing $RAM"
dd if=/dev/zero of=$RAM bs=$BS count=$ROOTBLOCKS
mkfs -B $BS -b $ROOTBLOCKS $RAM || exit
mkfs $TMPDISK2 || exit
echo " * mounting $RAM as $RELEASEDIR"
mount $RAM $RELEASEDIR || exit
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp
mount $TMPDISK2 $RELEASEDIR/tmp

echo " * Zeroing $TMPDISK"
dd if=/dev/zero of=$TMPDISK bs=$BS count=$USRBLOCKS
mkfs -B $BS -b $USRBLOCKS $TMPDISK || exit
echo " * Mounting $TMPDISK as $RELEASEDIR/usr"
mount $TMPDISK $RELEASEDIR/usr || exit
mkdir -p $RELEASEDIR/tmp
mkdir -p $RELEASEDIR/usr/tmp
echo " * Transfering $COPYITEMS to $RELEASEDIR"
( cd / && tar cf - $COPYITEMS ) | ( cd $RELEASEDIR && tar xf - ) || exit 1

if [ "$COPY" -ne 1 ]
then
	echo " * Doing new cvs export"
	( cd $RELEASEDIR/usr && mkdir src && cvs export -rHEAD src >/dev/null || exit 1 )
else
	( cd .. && make clean )
	echo " * Transfering source to $RELEASEDIR"
	( cd /usr && tar cf - src ) | ( cd $RELEASEDIR/usr && tar xf - )
fi

chown -R bin $RELEASEDIR/usr/src
find $RELEASEDIR/usr/src -type d | xargs chmod 755
find $RELEASEDIR/usr/src -type f | xargs chmod 644
# Bug tracking system not for on cd
rm -rf $RELEASEDIR/usr/src/doc/bugs
# No GNU core utils
rm -rf $RELEASEDIR/usr/src/contrib/gnu/coreutils*
# Make sure the CD knows it's a CD
date >$RELEASEDIR/CD
echo " * Chroot build"
chroot $RELEASEDIR '/bin/sh -x /usr/src/tools/chrootmake.sh' || exit 1
echo " * Chroot build done"
# The build process leaves some file in src as root.
chown -R bin $RELEASEDIR/usr/src
cp issue.install $RELEASEDIR/etc/issue

if [ "$HDEMU" -ne 0 ]; then hdemu_root_changes; fi

echo "Temporary filesystems still mounted. Make changes, or press RETURN"
echo -n "to continue making the image.."
read xyzzy
umount $TMPDISK || exit
umount $TMPDISK2 || exit
umount $RAM || exit
dd if=$RAM of=$ROOTIMAGE bs=$BS count=$ROOTBLOCKS
(cd ../boot && make)
make image || exit 1
cp image image_big
make clean
make image_small || exit 1
# Prepare image and image_small for cdfdboot
cp image_big image
sh mkboot cdfdboot
cp $IMAGE $CDFILES/bootflop.img
cp release/cd/* $CDFILES

h_opt=
bootimage=$IMAGE
if [ "$HDEMU" -ne 0 ]; then
	make_hdimage
	h_opt='-h'
	bootimage=hdimage
fi
writeisofs -l MINIX -b $bootimage $h_opt $CDFILES $ISO || exit 1

if [ "$HDEMU" -eq 0 ]
then
	echo "Appending Minix root and usr filesystem"
	( cat $ISO $ROOTIMAGE ; dd if=$TMPDISK bs=$BS count=$USRBLOCKS ) |
		gzip -9 >$ISOGZ || exit 1
else
	gzip -9 $ISO
fi
ls -al $ISOGZ
