#!/bin/sh
# Assume $1 is whitelist working directory
cd $1
mkdir -p $SRC/$1
while read dir
do
	cp -rf $dir $SRC/$1
done

cd $MINIX
