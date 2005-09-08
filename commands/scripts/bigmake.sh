#!/bin/sh
binsizes big
make $*
c=$?
binsizes normal
exit $c
