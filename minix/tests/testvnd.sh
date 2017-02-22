#!/bin/sh

# Shell script used to test the VNode Disk (VND) driver.

# The main purpose of this script is to test the ability of the driver to cope
# with regular operational conditions.  It also calls the blocktest test suite
# to perform I/O stress testing.  It does not aim to cover all of vndconfig(8).

bomb() {
  echo $*
  cd ..
  rm -rf $TESTDIR
  umount /dev/vnd1 >/dev/null 2>&1
  umount /dev/vnd0 >/dev/null 2>&1
  vndconfig -u vnd1 >/dev/null 2>&1
  vndconfig -u vnd0 >/dev/null 2>&1
  exit 1
}

PATH=/bin:/usr/bin:/sbin:/usr/sbin
export PATH

TESTDIR=DIR_VND
export TESTDIR

echo -n "Test VND "

# We cannot run the test if vnd0 or vnd1 are in use.
if vndconfig -l vnd0 >/dev/null 2>&1; then
  if ! vndconfig -l vnd0 2>/dev/null | grep "not in use" >/dev/null; then
    echo "vnd0 in use, skipping test" >&2
    echo "ok"
    exit 0
  else
    minix-service down vnd0
  fi
fi
if vndconfig -l vnd1 >/dev/null 2>&1; then
  if ! vndconfig -l vnd1 2>/dev/null | grep "not in use" >/dev/null; then
    echo "vnd1 in use, skipping test" >&2
    echo "ok"
    exit 0
  else
    minix-service down vnd1
  fi
fi

rm -rf $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

mkdir mnt0
mkdir mnt1

# Test persisting a file inside a vnode device.
dd if=/dev/zero of=image0 bs=4096 count=257 2>/dev/null || bomb "out of space?"
vndconfig vnd0 image0 || bomb "unable to configure vnd0"
[ "`devsize /dev/vnd0`" = 2056 ] || bomb "unexpected vnd0 device size"
mkfs.mfs /dev/vnd0 || bomb "unable to mkfs vnd0"
mount -t mfs /dev/vnd0 mnt0 >/dev/null || bomb "unable to mount vnd0"
STRING="Hello, world!"
echo "$STRING" >mnt0/TEST || bomb "unable to create file on vnd0"
umount /dev/vnd0 >/dev/null || bomb "unable to unmount vnd0"
[ ! -f mnt0/TEST ] || bomb "was vnd0 ever really mounted?"
mount -t mfs /dev/vnd0 mnt0 >/dev/null || bomb "unable to mount vnd0"
[ -f mnt0/TEST ] || bomb "test file was not saved across mounts"
[ "`cat mnt0/TEST`" = "$STRING" ] || bomb "test file was corrupted"
umount /dev/vnd0 >/dev/null || bomb "unable to unmount vnd0"
vndconfig -u vnd0 || bomb "unable to unconfigure vnd0"
vndconfig vnd1 image0 || bomb "unable to configure vnd1"
mount -t mfs /dev/vnd1 mnt1 >/dev/null || bomb "unable to mount vnd1"
[ "`cat mnt1/TEST`" = "$STRING" ] || bomb "test file was corrupted"

# Test nesting images.
dd if=/dev/zero of=mnt1/image1 bs=4096 count=128 2>/dev/null || bomb "dd fail"
vndconfig vnd0 mnt1/image1
mkfs.mfs /dev/vnd0 || bomb "unable to mkfs vnd0"
mount -t mfs /dev/vnd0 mnt0 >/dev/null || bomb "unable to mount vnd0"
echo "x" >mnt0/TEST2 || bomb "unable to create file on vnd0"
umount mnt1 >/dev/null 2>&1 && bomb "should not be able to unmount vnd1"
umount mnt0 >/dev/null || bomb "unable to unmount vnd0"
umount mnt1 >/dev/null 2>&1 && bomb "should not be able to unmount vnd1"
vndconfig -u vnd0 || bomb "unable to unconfigure vnd0"
cp mnt1/image1 . || bomb "unable to copy image off vnd1"
umount mnt1 >/dev/null || bomb "unable to unmount vnd1"
vndconfig -uS vnd1 || bomb "unable to unconfigure vnd1"
vndconfig -S vnd1 ./image1
mount -t mfs /dev/vnd1 mnt1 >/dev/null || bomb "unable to mount vnd1"
[ -f mnt1/TEST2 ] || bomb "test file not found, VM cache broken again?"
[ "`cat mnt1/TEST2`" = "x" ] || bomb "test file corrupted"
umount /dev/vnd1 >/dev/null || bomb "unable to unmount vnd1"
vndconfig -u /dev/vnd1 || bomb "unable to unconfigure vnd1"

# Test a read-only configuration.
SUM=`sha1 image0`
vndconfig -r vnd0 image0
dd if=/dev/zero of=/dev/vnd0 bs=4096 count=1 2>/dev/null && bomb "dd succeeded"
mount /dev/vnd0 mnt0 >/dev/null 2>&1 && bomb "mounting read-write should fail"
mount -r /dev/vnd0 mnt0 >/dev/null || bomb "unable to mount vnd0"
[ "`cat mnt0/TEST`" = "$STRING" ] || bomb "test file was corrupted"
dd if=/dev/zero of=/dev/vnd0 bs=4096 count=1 2>/dev/null && bomb "dd succeeded"
umount /dev/vnd0 >/dev/null
vndconfig -uS vnd0
[ "`sha1 image0`" = "$SUM" ] || bomb "read-only file changed"

# Test geometry and sub/partitions.
vndconfig -c vnd0 image0 512/32/64/2 2>/dev/null && bomb "vndconfig succeeded"
vndconfig -c vnd0 image0 512/32/64/1 || bomb "unable to configure vnd0"
# no need for repartition: nobody is holding the device open
[ "`devsize /dev/vnd0`" = 2048 ] || bomb "geometry not applied to size"
partition -mf /dev/vnd0 8 81:512 81:512* 81:512 81:1+ >/dev/null
partition -f /dev/vnd0p1 81:256 81:1 81:8 81:1+ >/dev/null
dd if=/dev/zero of=/dev/vnd0p1s2 bs=512 count=8 2>/dev/null || bomb "dd failed"
dd if=/dev/zero of=/dev/vnd0p1s2 bs=512 count=9 2>/dev/null && bomb "dd nofail"
mkfs.mfs /dev/vnd0p0 || bomb "unable to mkfs vnd0p1s3"
mkfs.mfs /dev/vnd0p1s3 || bomb "unable to mkfs vnd0p1s3"
mount /dev/vnd0p0 mnt0 >/dev/null || bomb "unable to mount vnd0p0"
mount /dev/vnd0p1s3 mnt1 >/dev/null || bomb "unable to mount vnd0p1s3"
umount /dev/vnd0p0 >/dev/null || bomb "unable to unmount vnd0p0"
umount mnt1 >/dev/null || bomb "unable to unmount vnd0p1s3"
vndconfig -u /dev/vnd0
vndconfig /dev/vnd1 image1 512/1/1/1025 2>/dev/null && bomb "can config vnd1"
vndconfig /dev/vnd1 image1 512/1/1/1024 2>/dev/null || bomb "can't config vnd1"
[ "`devsize /dev/vnd1`" = 1024 ] || bomb "invalid vnd1 device size"
dd if=/dev/vnd1 of=/dev/null bs=512 count=1024 2>/dev/null || bomb "dd fail"
dd if=/dev/vnd1 of=tmp bs=512 skip=1023 count=2 2>/dev/null
[ "`stat -f '%z' tmp`" = 512 ] || bomb "unexpected dd result"

# Test miscellaneous stuff.
vndconfig /dev/vnd1 image1 2>/dev/null && bomb "reconfiguring should not work"
# the -r is needed here to pass vndconfig(8)'s open test (which is buggy, too!)
vndconfig -r vnd0 . 2>/dev/null && bomb "config to a directory should not work"
vndconfig vnd0 /dev/vnd1 2>/dev/null && bomb "config to another vnd? horrible!"
diskctl /dev/vnd1 flush >/dev/null || bomb "unable to flush vnd1"
vndconfig -u vnd1

# Test the low-level driver API.
vndconfig vnd0 image0 || bomb "unable to configure vnd0"
../tvnd /dev/vnd0 || bomb "API subtest failed"
# the device is now unconfigured, but the driver is still running

# Invoke the blocktest test set to test various other aspects.
vndconfig -S vnd0 image0 || bomb "unable to configure vnd0"
cd ../blocktest
. ./support.sh
block_test /dev/vnd0 \
  "rw,min_read=1,min_write=1,element=1,max=16777216,nocontig,silent" || \
  bomb "blocktest test set failed"
vndconfig -u vnd0 || bomb "unable to unconfigure vnd0"

cd ..
rm -rf $TESTDIR

echo "ok"
exit 0
