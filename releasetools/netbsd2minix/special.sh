# Manually-copied files

for dir in "external bin games gnu libexec sbin usr.bin usr.sbin"
do
	cp -f $MINIX/$dir/Makefile $SRC/$dir
done

cp -r $MINIX/include/cdbr.h $SRC/include
cp -r $MINIX/share/zoneinfo $SRC/share
cp -r $MINIX/tools/llvm-librt $SRC/tools
cp -r $MINIX/tools/mkfs.mfs $SRC/tools
cp -r $MINIX/tools/mkproto $SRC/tools
cp -r $MINIX/tools/partition $SRC/tools
cp -r $MINIX/tools/toproto $SRC/tools
cp -r $MINIX/tools/writeisofs $SRC/tools
