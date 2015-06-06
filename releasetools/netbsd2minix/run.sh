#!/bin/sh
. releasetools/netbsd2minix/common.sh
$N2M/setupnetbsd.sh
$N2M/whitelists.sh
$N2M/setupminix.sh
cd $SRC
if [ `uname` -eq Minix ]
then
	make build
else
	./build.sh -m i386 build
