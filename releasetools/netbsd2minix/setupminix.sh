#!/bin/sh
cd ../..
SRC=releasetools/netbsd2minix/src

cp -r minix $SRC

rm $SRC/build.sh
cp build.sh $SRC

rm -r $SRC/distrib
cp -rdistrib $SRC

cd $SRC/..
