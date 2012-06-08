#!/bin/sh
diff=$3
rm -f "$diff"
if [ ! -d "$1" -o ! -d "$2" ]
then	echo Skipping $diff
	exit 0
fi
diff -aru $1 $2 | \
	sed /"^Only in"/d | \
	sed -e 's/^\(---.*\)\t.*/\1/' | \
	sed -e 's/^\(\+\+\+.*\)\t.*/\1/' > $diff

if [ ! -s "$diff" ]
then	rm -f "$diff"
fi
