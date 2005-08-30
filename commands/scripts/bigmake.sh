#!/bin/sh
minixsize big
make $*
c=$?
minixsize normal
exit $c
