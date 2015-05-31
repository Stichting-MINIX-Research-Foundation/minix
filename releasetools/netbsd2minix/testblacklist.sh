#!/bin/sh

echo "Testing blacklist..."
cd netbsd
SRC=`pwd`/src
while read bl
do
	echo $bl
	cd $SRC/$bl
	make	
done
cd ..
