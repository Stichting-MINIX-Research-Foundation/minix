#!/bin/sh

set -e

export CC=clang
export COMPILER_TYPE=gnu
export PATH=$PATH:/usr/pkg/bin

export MAKEOBJDIR=obj-elfbase-nbsd
make $@ NBSD_LIBC=yes
export MAKEOBJDIR=obj-elfbase
make $@ NBSD_LIBC=no
