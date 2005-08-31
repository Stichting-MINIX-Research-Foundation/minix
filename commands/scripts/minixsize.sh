#!/bin/sh
t=/usr/src/etc/binary_sizes
if [ "$1" = big ]
then t=$t.big
fi
chmem =250000 /usr/lib/* /usr/lib/i386/*
if [ -f $t ]
then	cat "$t" | while read line
	do	awk '{ print "chmem =" $2 " " $1 }'
	done | /bin/sh
fi
