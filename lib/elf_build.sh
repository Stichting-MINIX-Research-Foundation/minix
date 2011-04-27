#!/bin/sh

# This file is obsolete and is only useful to 'clean' its objects.

export CC=i386-pc-minix3-gcc
export COMPILER_TYPE=gnu
export MAKEOBJDIR=obj-elf
export PATH=$PATH:/usr/gnu_cross/bin
export NBSD_LIBC=${NBSD_LIBC}

if [ "$@" != clean ]
then	echo "$0: Unexpected arguments $@"
	exit 1
fi

make $@
