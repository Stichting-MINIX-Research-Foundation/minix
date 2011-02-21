#!/bin/sh

export CC=clang
export COMPILER_TYPE=gnu
export LD="i386-pc-minix3-ld"
export AR="i386-pc-minix3-ar"
export OBJCOPY="i386-pc-minix3-objcopy"
export RANLIB="i386-pc-minix3-ranlib"
export MAKEOBJDIR=obj-gnu
export PATH=$PATH:/usr/pkg/bin:/usr/gnu_cross/bin
export MAKEOBJDIR=obj-elf-clang

make $@
