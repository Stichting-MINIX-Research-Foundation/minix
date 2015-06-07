#!/bin/sh

# Current source tree
: ${MINIX=`pwd`}
# releasetools folder
: ${RT=$MINIX/releasetools}
# NetBSD source tree
: ${NETBSD=$MINIX/../netbsd}
# New source tree
: ${SRC=$MINIX/../newsrc}

export MINIX NETBSD SRC

echo "Setup NetBSD"
cd $NETBSD || (git clone --depth 1 git://github.com/jsonn/src $NETBSD && cd $NETBSD)
git pull

echo "Clear new src tree"
mkdir -p $SRC
rm -rf $SRC/*

echo "Apply whitelists"

cd $RT/whitelist
for item in `ls`
do
	$RT/applywhitelist.sh $NETBSD/$item $item < $item
	echo "$item moved"
done

$N2M/applywhitelist.sh $MINIX . < $RT/minix.txt

echo "Apply special-cases"

cd $MINIX

for dir in "external bin games gnu libexec sbin tools usr.bin usr.sbin"
do
	cp -f $MINIX/$dir/Makefile $SRC/$dir
done

cp -r $MINIX/include/cdbr.h $SRC/include/cdbr.h
cp -r $MINIX/share/zoneinfo $SRC/share/zoneinfo
cp -r $MINIX/tools/llvm-librt $SRC/tools/llvm-librt
cp -r $MINIX/tools/mkfs.mfs $SRC/tools/mkfs.mfs
cp -r $MINIX/tools/mkproto $SRC/tools/mkproto
cp -r $MINIX/tools/partition $SRC/tools/partition
cp -r $MINIX/tools/toproto $SRC/tools/toproto
cp -r $MINIX/tools/writeisofs $SRC/tools/writeisofs

