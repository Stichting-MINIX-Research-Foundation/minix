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

echo "Setup NetBSD"
$N2M/setupnetbsd.sh

echo "Clear new src tree"
mkdir -p $SRC
rm -rf $SRC/*

echo "Apply whitelists"
$N2M/whitelists.sh

echo "Apply special-cases"
$N2M/special.sh
