#!/bin/sh
#
#	M, U - mount or unmount standard devices.

case $#:$2 in
1:|2:-r)	;;
*)	echo "Usage: $0 <abbreviation> [-r]" >&2; exit 1
esac

. /etc/fstab

dev=$1 dir=$1

case $1 in
0)	dev=/dev/fd0 dir=fd0 ;;
1)	dev=/dev/fd1 dir=fd1 ;;
PS0|at0|fd0|pat0|pc0|ps0)	dev=/dev/$dev dir=fd0 ;;
PS1|at1|fd1|pat1|pc1|ps1)	dev=/dev/$dev dir=fd1 ;;
root)	dev=$root ;;
tmp)	dev=$tmp ;;
usr)	dev=$usr ;;
*)	dev=/dev/$dev dir=mnt
esac

case $0 in
*M)	mount $dev /$dir $2 ;;
*U)	umount $dev
esac
