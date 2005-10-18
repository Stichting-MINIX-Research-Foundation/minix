#!/bin/sh

t=/etc/binary_sizes

if [ "$#" -ne 1 ]
then	echo "Usage: $0 <big|normal>"
	exit 1
fi

if [ "$1" = big ]
then t=$t.big
fi
chmem =250000 /usr/lib/* /usr/lib/i386/* >/dev/null 2>&1
chmem =600000 /usr/lib/ego/*  >/dev/null 2>&1
if [ -f $t ]
then	cat "$t" | while read line
	do	awk '{ print "chmem =" $2 " " $1 " >/dev/null 2>&1 "}'
	done | /bin/sh
fi
