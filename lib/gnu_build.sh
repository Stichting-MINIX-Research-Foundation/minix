#!/bin/sh

export COMPILER_TYPE=gnu
export ARCH=i386

if [ "$COMPILER_TYPE" = 'ack' ]; then
	export CC=cc
	export MAKEOBJDIR=obj-ack
elif [ "$COMPILER_TYPE" = 'gnu' ]; then
	export CC=gcc
	export AR=gar
	export MAKEOBJDIR=obj-gnu
	export PATH=$PATH:/usr/gnu/bin
fi

make $@
