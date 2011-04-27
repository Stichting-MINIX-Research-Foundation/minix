#!/bin/sh

export CC=clang
export COMPILER_TYPE=gnu
export PATH=$PATH:/usr/pkg/bin:/usr/gnu_cross/bin
export MAKEOBJDIR=obj-elf-clang
export NBSD_LIBC=${NBSD_LIBC}

make $@
