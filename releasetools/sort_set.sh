#!/bin/sh
#
# kinda natural sorting for sets
# prepend every line with a modified path where
# slashes are replaced by "1" and "0" is added
# at the end of the string.
# 
# entry
#  ./bin/cat minix-sys 
# becomes
#.1bin1cat0 ./bin/cat minix-sys 
#
# This entry gets sorted after wich the key is removed using
# cut
#
# Additionally all lines starting with "#" are put on
# top in the order they where put in the file. this is done
# by creating a "key" with the value COUNTER
# 
COUNTER=10000
while read i 
do
	A=$(echo $i | cut -f 1 -d ' ' | sed "s,^#,00$COUNTER,g" | sed 's,/,1,g' )
	echo "${A}0 $i"
	COUNTER=$(($COUNTER +1))
done  | sort | cut -d ' ' -f 2-
