#!/bin/sh
# Assume $1 is whitelist working directory
cd $1
while read dir
do
	cp -rf $dir $SRC/$2
done

cd $MINIX
