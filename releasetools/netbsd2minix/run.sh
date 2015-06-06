#!/bin/sh

# Current source tree
MINIX=`pwd`
# NetBSD2Minix folder
N2M=$MINIX/releasetools/netbsd2minix
# NetBSD source tree
NETBSD=$N2M/netbsd
# New source tree
SRC=$N2M/src

export MINIX N2M NETBSD SRC


$N2M/setupnetbsd.sh
mkdir -p $SRC
rm -rf $SRC/*
$N2M/whitelists.sh
cd $SRC

./build.sh -m i386 build
