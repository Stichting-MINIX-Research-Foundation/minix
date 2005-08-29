#!/bin/sh
cat "$1" | while read line
do	awk '{ print "chmem =" $2 " " $1 }'
done | /bin/sh
