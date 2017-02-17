#!/bin/sh

# expected sha1sum of the FS image
expect=98bcafa04cb1eb75b7add6c95eb587c37f5050e0

set -e

# ownership matters for the proto file.
# the run script runs us with user "bin" (3), group "bin" (7).
if [ "`id -u`" != 3 -o "`id -g`" != 7 ]
then
	echo "test script should be run with uid 3, gid 7."
	exit 1
fi

echo -n "mfs test "

testdir=fstest
protofile=proto
fsimage=fsimage
rm -rf $testdir $protofile $fsimage

if [ -d $testdir ]
then
	echo "dir?"
	exit 1
fi

mkdir -p $testdir $testdir/contents $testdir/modes

if [ ! -d $testdir ]
then
	echo "no dir?"
	exit 1
fi

# Make some small & big & bigger files

prevf=$testdir/contents/file
echo "Test contents 123" >$prevf
for double in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do	fn=$testdir/contents/fn.$double
	cat $prevf $prevf >$fn
	prevf=$fn
done

# Make some files with various modes & mtimes

for many in 0 1 2 3 4 5 6 7 8 9
do	for m1 in 0 1 2 3 4 5 6 7
	do	for m2 in 0 1 2 3 4 5 6 7
		do 	for m3 in 0 1 2 3 4 5 6 7
			do
				mode=${m1}${m2}${m3} 
				fn=$testdir/modes/m${mode}${many}
				echo "$many $m1 $m2 $m3 $mode" > $fn
				chmod $mode $fn
			done
		done
	done
done

# Make an MFS filesystem image out of it

BS=4096
BLOCKS=15000
INODES=6000
dd if=/dev/zero seek=$BLOCKS of=$fsimage count=1 bs=$BS >/dev/null 2>&1

# -s keeps modes
/usr/sbin/mkproto -s -b $BLOCKS -i $INODES $testdir >$protofile

/sbin/mkfs.mfs -T 1 -b $BLOCKS -i $INODES  $fsimage $protofile >/dev/null 2>&1
sum="`sha1 $fsimage | awk '{ print $4 }'`"

rm -rf $testdir $protofile $fsimage

if [ $sum != $expect ]
then	
	echo sum $sum is not expected $expect
	exit 1
fi

echo ok

exit 0

