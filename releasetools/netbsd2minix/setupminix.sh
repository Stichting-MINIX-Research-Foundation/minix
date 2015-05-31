#!/bin/sh
cd ../..
SRC=releasetools/netbsd2minix/src

cp minix $SRC

rm $SRC/build.sh
cp build.sh $SRC

rm -r $SRC/distrib
cp distrib $SRC

cd $SRC/..
