#!/bin/sh

set -e

export CC=clang
export COMPILER_TYPE=gnu
export MAKEOBJDIR=obj-elf-base
export PATH=$PATH:/usr/pkg/bin

make $@
