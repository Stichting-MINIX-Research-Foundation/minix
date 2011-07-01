#!/bin/sh

set -e

export CC=${CC:-clang}
export COMPILER_TYPE=gnu
export PATH=$PATH:/usr/pkg/bin

export MAKEOBJDIR=obj-elfbase-nbsd NBSD_LIBC=yes
make $@
