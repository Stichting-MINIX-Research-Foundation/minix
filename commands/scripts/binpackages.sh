#!/bin/sh

pdir=Packages

for d in */build
do
	dir="`echo $d | sed 's/\/build$//'`"
	if [ ! -f $pdir/$dir.tar.gz ]
	then
		echo " * $dir"
		binpackage $dir $pdir
	fi
done
