#!/bin/sh

export CC=clang
export COMPILER_TYPE=gnu
export MAKEOBJDIR=obj-gnu
export PATH=$PATH:/usr/pkg/bin

make $@
