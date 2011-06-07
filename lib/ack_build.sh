#!/bin/sh

set -e

export CC=cc
export MAKEOBJDIR=obj-ack

make $@
