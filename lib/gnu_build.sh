#!/bin/sh

export CC=gcc
export MAKEOBJDIR=obj-gnu
export PATH=$PATH:/usr/gnu/bin

make $@
