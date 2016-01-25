# Create and verify a simple ISO filesystem
#
#!/bin/sh

set -e

echo -n "isofs test "

# Somehow timezones mess up the timestamp comparison, so unset the timezone for
# now.  TODO: sort out if this is actually a bug or simply expected behavior.
unset TZ

# testing ISO 9660 Level 3 compliance isn't possible for the time being
# (not possible to store a >4GB ISO file into a ramdisk)
testLevel3=0
testRockRidge=1

ramdev=/dev/ram
mp=/mnt
testdir=isofstest
fsimage=isofsimage
contents=CONTENTS
out1=v1
out2=v2
excludes=excludes

create_contents_level3() {
	# >4GB file
	seq 1 1000000000 > $testdir/HUGEFILE
}

create_contents_rockridge() {
	# long filenames
	mkdir -p $testdir/rockridge/longnames
	echo "this is a test" > $testdir/rockridge/longnames/azertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopaz
	echo "this is a test" > $testdir/rockridge/longnames/azertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopaze
	echo "this is a test" > $testdir/rockridge/longnames/azertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazertyuiopazer

	# devices
	mkdir -p $testdir/rockridge/devices
	CURLOC=$(pwd)
	cd $testdir/rockridge/devices && MAKEDEV -s
	cd $CURLOC

	# symbolic links
	mkdir -p $testdir/rockridge/symlinks
	ln -s . $testdir/rockridge/symlinks/cur_dir
	ln -s .. $testdir/rockridge/symlinks/parent_dir
	ln -s / $testdir/rockridge/symlinks/root_dir
	ln -s /mnt $testdir/rockridge/symlinks/root_mnt_dir
	ln -s ../../rockridge $testdir/rockridge/symlinks/rockridge_dir
	ln -s ../../rockridge/symlinks $testdir/rockridge/symlinks/symlinks_dir
	ln -s ../../rockridge/symlinks/../symlinks $testdir/rockridge/symlinks/symlinks_dir_bis
	ln -s cur_dir $testdir/rockridge/symlinks/to_cur_dir
	ln -s rockridge_dir $testdir/rockridge/symlinks/to_rockridge_dir

	# deep directory tree
	mkdir -p $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think
	mkdir -p $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/yes
	mkdir -p $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/no
	echo "I agree." > $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/yes/awnser1
	echo "Yes, totally." > $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/yes/awnser2
	echo "Nah." > $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/no/awnser1
	echo "Meh." > $testdir/rockridge/deep_dirs/this/is/a/ridiculously/deep/directory/hierarchy/dont/you/think/no/awnser2

	# permissions
	mkdir -p $testdir/rockridge/permissions
	for u in $(seq 0 7); do
		for g in $(seq 0 7); do
			for o in $(seq 0 7); do
				echo "$u$g$o" > $testdir/rockridge/permissions/mode-$u$g$o
				chmod $u$g$o $testdir/rockridge/permissions/mode-$u$g$o
			done
		done
	done
	echo "uid-gid test" > $testdir/rockridge/permissions/uid-1-gid-2
	chown 1:2 $testdir/rockridge/permissions/uid-1-gid-2
	echo "uid-gid test" > $testdir/rockridge/permissions/uid-128-gid-256
	chown 128:256 $testdir/rockridge/permissions/uid-128-gid-256
	echo "uid-gid test" > $testdir/rockridge/permissions/uid-12345-gid-23456
	chown 12345:23456 $testdir/rockridge/permissions/uid-12345-gid-23456
}

create_contents_base() {
	# simple file
	echo $(date) > $testdir/DATE

	# big file
	seq 1 100000 > $testdir/BIGFILE

	# lots of files in a directory
	mkdir $testdir/BIGDIR
	for i in $(seq 1 250); do
		HASH=$(cksum -a SHA1 <<EOF
$i
EOF
)
		FILE=$(echo $HASH | cut -c 1-30 | sed -e "y/abcdef/ABCDEF/")
		echo $HASH > $testdir/BIGDIR/$FILE
	done

	# lots of directories
	mkdir $testdir/SUBDIRS
	for i in $(seq 1 1000); do
		HASH=$(cksum -a SHA1 <<EOF
$i
EOF
)
		DIR1=$(echo $HASH | cut -c 1-2 | sed -e "y/abcdef/ABCDEF/")
		DIR2=$(echo $HASH | cut -c 3-4 | sed -e "y/abcdef/ABCDEF/")
		FILE=$(echo $HASH | cut -c 5-12 | sed -e "y/abcdef/ABCDEF/")
		mkdir -p $testdir/SUBDIRS/$DIR1/$DIR2
		echo $HASH > $testdir/SUBDIRS/$DIR1/$DIR2/$FILE
	done
}

rm -rf $testdir $fsimage $out1 $out2 $excludes

if [ -d $testdir ]
then
	echo "dir?"
	exit 1
fi

mkdir -p $testdir

if [ ! -d $testdir ]
then
	echo "no dir?"
	exit 1
fi

# make some small & big & bigger files
OPTIONS=
create_contents_base
if [ "$testLevel3" -eq 1 ]
then
	create_contents_level3
fi
if [ "$testRockRidge" -eq 1 ]
then
	create_contents_rockridge
	OPTIONS="-o rockridge"
else
	# fixups for the fact that bare ISO 9660 isn't POSIX enough
	# for mtree
	# fix permissions
	find $testdir -exec chmod 555 {} ";"
fi

# make image
/usr/sbin/makefs -t cd9660 $OPTIONS $fsimage $testdir

# umount previous things
umount $ramdev >/dev/null 2>&1 || true
umount $mp >/dev/null 2>&1 || true

# mount it on a RAM disk
ramdisk $(expr $(wc -c < $fsimage) / 1024) $ramdev >/dev/null 2>&1
cat < $fsimage > $ramdev
mount -t isofs $ramdev $mp >/dev/null 2>&1

# compare contents
if [ "$testRockRidge" -eq 1 ]
then
	# get rid of root directory time
	echo 'RR_MOVED' >$excludes
	/usr/sbin/mtree -c -p $testdir | sed -e "s/\. *type=dir.*/\. type=dir/" | /usr/sbin/mtree -p $mp -X $excludes
else
	# fixups for the fact that bare ISO 9660 isn't POSIX enough
	# for mtree
	# get rid of time
	/usr/sbin/mtree -c -p $testdir | sed -e "s/time=[0-9]*.[0-9]*//" | /usr/sbin/mtree -p $mp
fi

umount $ramdev >/dev/null 2>&1

# cleanup
rm -rf $testdir $fsimage $out1 $out2 $excludes

echo ok

exit 0
