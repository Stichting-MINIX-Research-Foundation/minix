#!/bin/sh

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
echo "Note: this script wants to do cvs operations, so it's necessary"
echo "to have \$CVSROOT set and cvs login done."
echo ""
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

umount $TMPDISK
umount $RAM

( cd .. && make clean )
( cd .. && make depend )
echo " * Cleanup old files"
rm -rf $RELEASEDIR $ISO $IMAGE $ROOTIMAGE $ISOGZ $CDFILES
mkdir -p $CDFILES || exit
mkdir -p $RELEASEDIR
echo " * Zeroing $RAM"
dd if=/dev/zero of=$RAM bs=$BS count=$ROOTBLOCKS
mkfs -B $BS -b $ROOTBLOCKS $RAM || exit
echo " * mounting $RAM as $RELEASEDIR"
mount $RAM $RELEASEDIR || exit
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp

echo " * Zeroing $TMPDISK"
dd if=/dev/zero of=$TMPDISK bs=$BS count=$USRBLOCKS
mkfs -B $BS -b $USRBLOCKS $TMPDISK || exit
echo " * Mounting $TMPDISK as $RELEASEDIR/usr"
mount $TMPDISK $RELEASEDIR/usr || exit
mkdir -p $RELEASEDIR/tmp
mkdir -p $RELEASEDIR/usr/tmp
echo " * Transfering $COPYITEMS to $RELEASEDIR"
( cd / && tar cf - $COPYITEMS ) | ( cd $RELEASEDIR && tar xf - ) || exit 1
echo " * Doing new cvs export"
( cd $RELEASEDIR/usr && mkdir src && cvs export -rHEAD src >/dev/null 2>&1 || exit 1 )
chown -R bin $RELEASEDIR/usr/src
# Bug tracking system not for on cd
rm -rf $RELEASEDIR/usr/src/doc/bugs
# No GNU core utils
rm -rf $RELEASEDIR/usr/src/contrib/gnu/coreutils*
# Make sure the CD knows it's a CD
date >$RELEASEDIR/CD
echo " * Chroot build"
chroot $RELEASEDIR '/bin/sh -x /usr/src/tools/chrootmake.sh' || exit 1
echo " * Chroot build done"
cp issue.install $RELEASEDIR/etc/issue
umount $TMPDISK || exit
umount $RAM || exit
dd if=$RAM of=$ROOTIMAGE bs=$BS count=$ROOTBLOCKS
make programs image
(cd ../boot && make)
make image || exit 1
sh mkboot cdfdboot
cp $IMAGE $CDFILES/bootflp.img
cp release/cd/* $CDFILES
writeisofs -l MINIX -b $IMAGE $CDFILES $ISO || exit 1
echo "Appending Minix root and usr filesystem"
( cat $ISO $ROOTIMAGE ; dd if=$TMPDISK bs=$BS count=$USRBLOCKS ) | gzip -9 >$ISOGZ || exit 1
ls -al $ISOGZ
