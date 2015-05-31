#!/bin/sh
cd ../..
SRC=releasetools/netbsd2minix/src

cp -r minix $SRC

rm $SRC/build.sh
cp build.sh $SRC

cp -r distrib $SRC

cd $SRC/..
