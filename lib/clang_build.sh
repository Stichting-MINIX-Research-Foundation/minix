#!/bin/sh

export CC=clang
export COMPILER_TYPE=gnu
export MAKEOBJDIR=obj-gnu
export PATH=$PATH:/usr/gnu/bin:/usr/llvm/bin

make $@
