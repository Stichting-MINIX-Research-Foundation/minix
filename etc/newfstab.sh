#!/bin/sh

set -e

if [ $# -ne 1 ]
then	echo "Usage: $0 <minix-style-fstab> >newfstab"
	exit 1
fi

fstab="$1"
. $fstab

if [ -z "$usr" -o -z "$root" ]
then	echo "\$root and \$usr not set in $fstab"
	exit 1
fi

echo "$root	/	mfs	rw	0	2"
echo "$usr	/usr	mfs	rw	0	1"
if [ -n "$home" ]
then	echo "$home	/home	mfs	rw	0	1"
fi
