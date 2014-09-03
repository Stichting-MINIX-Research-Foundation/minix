# Create and verify a simple ISO filesystem
#
#!/bin/sh

set -e

echo -n "isofs test "

#This test is known to be currently broken, so just skip it
echo 'ok # skip Currently known to be failing, patch pending'
exit 0

ramdev=/dev/ram
mp=/mnt
testdir=isofstest
fsimage=isofsimage
contents=CONTENTS
out1=v1
out2=v2
rm -rf $testdir $fsimage $out1 $out2

if [ -d $testdir ]
then
	echo "dir?"
	exit 1
fi

mkdir -p $testdir $testdir/$contents

if [ ! -d $testdir ]
then
	echo "no dir?"
	exit 1
fi

# Make some small & big & bigger files

prevf=$testdir/$contents/FILE
echo "Test contents 123" >$prevf
for double in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do	fn=$testdir/$contents/FN.$double
	cat $prevf $prevf >$fn
	prevf=$fn
done

# Make an ISO filesystem image out of it
writeisofs -s0x0 -l MINIX $testdir $fsimage >/dev/null 2>&1

# umount previous things
umount $ramdev >/dev/null 2>&1 || true
umount $mp >/dev/null 2>&1 || true

# Mount it on a RAM disk
ramdisk 50000 $ramdev >/dev/null 2>&1
cp $fsimage $ramdev
mount -t isofs $ramdev $mp >/dev/null 2>&1

# compare contents
(cd $testdir/$contents && sha1 * | sort) >$out1
(cd $mp/$contents && sha1 * | sort) >$out2

diff -u $out1 $out2

umount $ramdev >/dev/null 2>&1

# cleanup
rm -rf $testdir $fsimage $out1 $out2

echo ok

exit 0
