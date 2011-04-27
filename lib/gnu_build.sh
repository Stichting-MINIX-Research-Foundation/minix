#!/bin/sh

export CC=gcc
export PATH=$PATH:/usr/pkg/bin

export MAKEOBJDIR=obj-gnu-nbsd
make $@ NBSD_LIBC=yes
export MAKEOBJDIR=obj-gnu
make $@ NBSD_LIBC=no
