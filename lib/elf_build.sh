#!/bin/sh

export CC=i386-pc-minix3-gcc
export COMPILER_TYPE=gnu
export MAKEOBJDIR=obj-elf
export PATH=$PATH:/usr/gnu_cross/bin

make $@
