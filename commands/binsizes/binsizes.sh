#!/bin/sh

PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/xbin

t=/etc/binary_sizes

if [ "$#" -ne 1 ]
then	echo "Usage: $0 <big|normal|xxl>"
	exit 1
fi

if [ "$1" != normal ]
then	t=$t.$1
fi

chmem =250000 /usr/lib/em_* /usr/lib/cpp* /usr/lib/cv >/dev/null 2>&1
chmem =600000 /usr/lib/ego/*  >/dev/null 2>&1
if [ -f $t ]
then	cat "$t" | while read line
	do	awk '{ print "chmem =" $2 " " $1 " 2>&1 | grep -v area.changed.from  || exit 1"}'
	done | /bin/sh -e || exit 1
else
	echo "$0: $t does not exist" >&2
	exit 1
fi
exit 0
