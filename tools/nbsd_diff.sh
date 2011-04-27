#!/bin/sh
echo "Updating $3"
diff -ru $1 $2 | \
	sed /"^Only in"/d | \
	sed -e 's/^\(---.*\)\t.*/\1/' | \
	sed -e 's/^\(\+\+\+.*\)\t.*/\1/' > $3
