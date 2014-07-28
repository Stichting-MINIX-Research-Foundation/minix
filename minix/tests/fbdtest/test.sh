#!/bin/sh

# This test set tests the some of the basic functionality of the Faulty Block
# Device driver. It takes a writable device as input - a small (sub)partition
# suffices for this purpose. All information on the given device WILL BE LOST,
# so USE AT YOUR OWN RISK.
#
# Currently, a reasonable subset of supported read and write fault injection is
# tested. Since injection of write faults was the original goal for this
# driver, the test set for this part of FBD functionality is relatively large.
#
# Testing of read faults works as follows. First, a known pattern is written to
# the actual device. Then FBD is loaded as an overlay over the device. A fault
# injection rule is set on FBD, and the disk pattern is read back from the FBD
# device (/dev/fbd). FBD is then unloaded. The test succeeds if the pattern
# that was read back, matches a certain expected pattern.
#
# Testing of write faults works as follows. First, a known pattern is written
# to the actual device. Then FBD is loaded as an overlay over the device. A
# fault injection rule is set on FBD, and another pattern is written to the FBD
# device (/dev/fbd). FBD is unloaded, and the resulting disk pattern is read
# back from the actual device. This resulting pattern should match a certain
# expected pattern.
#
# Since all raw block I/O requests go through the root file server, this test
# set heavily depends on the behavior of that root file server. It has been
# tested with MFS, and may not work with any other file server type. It assumes
# that a 4K block size is used, and that the file server translates raw block
# requests to aligned 4K-multiples. The test set also makes assumptions about
# merging pages in write operations, flushing only upon a sync call, etcetera.
# Unfortunately, this dependency on the root file server precludes the test set
# from properly exercising all possible options of FBD.

RWBLOCKS=./rwblocks

devtopair() {
  label=`awk "/^$(stat -f '%Hr' $1) / "'{print $2}' /proc/dmap`
  if [ ! -z "$label" ]; then echo "label=$label,minor=`stat -f '%Lr' $1`"; fi
}

if [ ! -b "$1" ]; then
  echo "usage: $0 device" >&2
  exit 1
fi

PAIR=$(devtopair $1)
if [ -z "$PAIR" ]; then
  echo "driver not found for $1" >&2
  exit 1
fi

if [ ! -x $RWBLOCKS ]; then
  make || exit 1
fi

if [ "`stat -f '%k' /`" != "4096" ]; then
  echo "The root file system is not using a 4K block size." >&2
  exit 1
fi

read -p "This will overwrite the contents of $1. Are you sure? [y/N] " RESP
case $RESP in
  [yY]*)
    ;;
  *)
    echo "Hmpf. Okay. Aborting test.."
    exit 0
esac

DEV="$1"
LAST=
SUCCESS=0
TOTAL=0

read_test() {
  OPT=
  if [ "$1" = "-last" -o "$1" = "-notlast" ]; then
    OPT=$1
    shift
  fi
  PAT=$1
  EXP=$2
  shift 2
  $RWBLOCKS $DEV $PAT
  service up /usr/sbin/fbd -dev /dev/fbd -args "$PAIR" || exit 1
  fbdctl add $@ >/dev/null
  #fbdctl list
  RES="`$RWBLOCKS /dev/fbd`"
  service down fbd
  echo -n "$RES: "
  if echo "$RES" | egrep "^$EXP\$" >/dev/null 2>&1; then
    if [ "$OPT" = "-last" -a "$RES" != "$LAST" ]; then
      echo FAILURE
    elif [ "$OPT" = "-notlast" -a "$RES" = "$LAST" ]; then
      echo FAILURE
    else
      echo SUCCESS
      SUCCESS=`expr $SUCCESS + 1`
      LAST="$RES"
    fi
  else
    echo FAILURE
  fi
  TOTAL=`expr $TOTAL + 1`
}

write_test() {
  OPT=
  if [ "$1" = "-last" -o "$1" = "-notlast" ]; then
    OPT=$1
    shift
  fi
  PAT=$1
  EXP=$2
  WS=$3
  shift 3
  $RWBLOCKS $DEV UUUUUUUUUUUUUUUU
  service up /usr/sbin/fbd -dev /dev/fbd -args "$PAIR" || exit 1
  fbdctl add $@ >/dev/null
  #fbdctl list
  $RWBLOCKS /dev/fbd $PAT $WS
  service down fbd
  RES="`$RWBLOCKS $DEV`"
  echo -n "$RES: "
  if echo "$RES" | egrep "^$EXP\$" >/dev/null 2>&1; then
    if [ "$OPT" = "-last" -a "$RES" != "$LAST" ]; then
      echo FAILURE
    elif [ "$OPT" = "-notlast" -a "$RES" = "$LAST" ]; then
      echo FAILURE
    else
      echo SUCCESS
      SUCCESS=`expr $SUCCESS + 1`
      LAST="$RES"
    fi
  else
    echo FAILURE
  fi
  TOTAL=`expr $TOTAL + 1`
}

read_test AAAAAAAAAAAAAAAA A0AAAAAAAAAAAAAA -a 1000-2000 -r corrupt zero

read_test       AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA' -a 2000-4000 -r corrupt persist
read_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA' -a 2000-4000 -r corrupt persist

read_test          AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA' -a 5000-8000 -r corrupt random
read_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA' -a 5000-8000 -r corrupt random

read_test AAAAAAAAAAAAAAAA 'A[a-z]AAAAAAAAAAAAAA' -a 1100-1200 -r corrupt zero

read_test AAAAAAAAAAAAAAAA 'AA#AAAAAAAAAAAAA' -a 2000-3000 -r error EIO
read_test AAAAAAAAABAAABAA 'AAAAAAAAAB###BAA' -a A800-C800 -r error EIO

read_test ABBBAAAAAAAAAAAA 'ABBB#' -a 4000 -r error OK

write_test AAAAAAAAAAAAAAAA A0AAAAAAAAAAAAAA   512 -a 1000-2000 -w corrupt zero
write_test AAAAAAAAAAAAAAAA A0AAAAAAAAAAAAAA  4096 -a 1000-2000 -w corrupt zero
write_test AAAAAAAAAAAAAAAA A0AAAAAAAAAAAAAA 16384 -a 1000-2000 -w corrupt zero

write_test       AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA'   512 -a 2000-4000 -w corrupt persist
write_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA'   512 -a 2000-4000 -w corrupt persist
write_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA'  4096 -a 2000-4000 -w corrupt persist
write_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA'  4096 -a 2000-4000 -w corrupt persist
write_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA' 16384 -a 2000-4000 -w corrupt persist
write_test -last AAAAAAAAAAAAAAAA 'AA[a-z][a-z]AAAAAAAAAAAA' 16384 -a 2000-4000 -w corrupt persist

write_test          AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA'   512 -a 5000-8000 -w corrupt random
write_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA'   512 -a 5000-8000 -w corrupt random
write_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA'  4096 -a 5000-8000 -w corrupt random
write_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA'  4096 -a 5000-8000 -w corrupt random
write_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA' 16384 -a 5000-8000 -w corrupt random
write_test -notlast AAAAAAAAAAAAAAAA 'AAAAA[a-z][a-z][a-z]AAAAAAAA' 16384 -a 5000-8000 -w corrupt random

write_test AAAAAAAAAAAAAAAA 'A[a-z]AAAAAAAAAAAAAA'   512 -a 1100-1200 -w corrupt zero
write_test AAAAAAAAAAAAAAAA 'A[a-z]AAAAAAAAAAAAAA'  4096 -a 1100-1200 -w corrupt zero
write_test AAAAAAAAAAAAAAAA 'A[a-z]AAAAAAAAAAAAAA' 16384 -a 1100-1200 -w corrupt zero

write_test AAAAAAAAAAAAAAAA AAAUUUUUUUUUUUUU   512 -a 3000 -w error EIO
write_test AAAAAAAAAAAAAAAA AAAUUUUUUUUUUUUU  4096 -a 3000 -w error EIO
write_test AAAAAAAAAAAAAAAA AAAUUUUUUUUUUUUU 16384 -a 3000 -w error EIO

write_test AAAAAAAAAAAAABAA AAAAAABAAAAAAUAA        4096 -a D000-E000 -w misdir 6000-7000 4096
write_test AAAAAAAAAAAAABAA 'AAAAAA(AB|BA)AAAAAUAA' 4096 -a D000-E000 -w misdir 6000-8000 4096
write_test AAAAAAAAAAAAABAA 'AAAAAA(AB|BA)AAAAAUAA' 4096 -a D000-E000 -w misdir 6000-8000 4096
write_test AAAAAAAAAAAAABAA 'AAAAAA(AB|BA)AAAAAUAA' 4096 -a D000-E000 -w misdir 6000-8000 4096

write_test AAAAAAAAABAAAAAA AAAAAAAAAUAAAAAA   512 -a 9000-A000 -w lost
write_test AAAAAAAAABAAAAAA AAAAAAAAAUAAAAAA  4096 -a 9000-A000 -w lost
write_test AAAAAAAAABAAAAAA AAAAAAAAUUUUAAAA 16384 -a 9000-A000 -w lost

write_test AAAAAAAAAAABAAAA 'AAAAAAAAAAA[a-z]AAAA'   512 -a B000-C000 -w torn 512
write_test AAAAAAAAAAABAAAA 'AAAAAAAAAAA[a-z]AAAA'  4096 -a B000-C000 -w torn 512
write_test AAAAAAAAAAABAAAA 'AAAAAAAA[a-z]UUUAAAA' 16384 -a B000-C000 -w torn 512

write_test AAAAAAAAAAABAAAA AAAAAAAAAAABAAAA   512 -a B000-C000 -w torn 4096
write_test AAAAAAAAAAABAAAA AAAAAAAAAAABAAAA  4096 -a B000-C000 -w torn 4096
write_test AAAAAAAAAAABAAAA AAAAAAAAAUUUAAAA 16384 -a B000-C000 -w torn 4096

echo "$SUCCESS out of $TOTAL tests succeeded."
