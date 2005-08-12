#!/bin/sh

RM="rm -f"
MV="mv -f"

if [ $# -ne 2 ]
then	echo "Usage: $0 <log> <keep>"
	exit 1
fi

if [ ! -f "$1" ]
then	 exit 1
fi

if [ "$2" -le 0 ]
then	echo "Keep at least 1 copy please."
	exit 1
fi

k="$2"
$RM "$1.$k" 2>/dev/null || exit 1
while [ "$k" -ge 2 ]
do	prev="`expr $k - 1`"
	$MV $1.$prev.gz $1.$k.gz 2>/dev/null 
	k=$prev
done
gzip -c $1 >$1.1.gz && : >$1
