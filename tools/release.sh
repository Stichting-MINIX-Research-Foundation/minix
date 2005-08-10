#!/bin/sh

COPYITEMS="usr/src usr/bin bin usr/lib"
RELEASEDIR=/usr/r/release
IMAGE=cdfdimage
ROOTIMAGE=rootimage
CDFILES=/usr/tmp/cdreleasefiles
ISO=minix.iso
ISOGZ=minix.iso.gz
RAM=/dev/ram
BS=4096
rootmb=2
rootkb=`expr $rootmb \* 1024`
rootbytes=`expr $rootkb \* 1024`
if [ `wc -c $RAM | awk '{ print $1 }'` -ne $rootbytes ]
then	echo "$RAM should be exactly ${rootkb}k."
	exit 1
fi
echo "Warning: I'm going to mkfs $RAM!"
echo "Temporary (sub)partition to use to make the /usr FS image? "
echo "It will be mkfsed!"
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
mkfs -B $BS $RAM || exit
echo " * chowning to bin"
chown -R bin /usr/src
echo " * mounting $RAM as $RELEASEDIR"
mount $RAM $RELEASEDIR || exit
mkdir -m 755 $RELEASEDIR/usr
mkdir -m 1777 $RELEASEDIR/tmp

mkfs -B $BS $TMPDISK || exit
echo " * mounting $TMPDISK as $RELEASEDIR/usr"
mount $TMPDISK $RELEASEDIR/usr || exit
mkdir -p $RELEASEDIR/tmp
mkdir -p $RELEASEDIR/usr/tmp
echo " * Transfering $COPYITEMS to $RELEASEDIR"
( cd / && tar cf - $COPYITEMS ) | ( cd $RELEASEDIR && tar xf - ) || exit 1
chown -R bin $RELEASEDIR/usr/src
# Bug tracking system not for on cd
rm -rf $RELEASEDIR/usr/src/doc/bugs
date >$RELEASEDIR/CD
( cd $RELEASEDIR && find . -name CVS | xargs rm -rf )
#echo " * Making source .tgz for on ISO filesystem"
#( cd $RELEASEDIR/usr/src && tar cf - . | gzip > $CDFILES/MINIXSRC.TGZ )
echo " * Chroot build"
chroot $RELEASEDIR '/bin/sh -x /usr/src/tools/chrootmake.sh' || exit 1
echo " * Chroot build done"
cp issue.install $RELEASEDIR/etc/issue
umount $TMPDISK || exit
umount $RAM || exit
cp $RAM $ROOTIMAGE
make programs image
(cd ../boot && make)
make image || exit 1
sh mkboot cdfdboot
cp $IMAGE $CDFILES/bootflp.img
cp release/cd/* $CDFILES
writeisofs -l MINIX -b $IMAGE $CDFILES $ISO || exit 1
echo "Appending Minix root and usr filesystem"
cat $ISO $ROOTIMAGE $TMPDISK | gzip >$ISOGZ || exit 1
ls -al $ISOGZ
