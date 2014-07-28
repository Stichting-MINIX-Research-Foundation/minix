#!/bin/sh

set -e

# grep message type and errno constants and make them into a .h file
cat ../../sys/sys/errno.h | \
tr -s ' \t' ' ' | \
sed 's/^# /#/' | \
egrep '^#define [A-Z_][A-Z0-9_]* \( ?_SIGN ?[0-9]+ ?\)' | \
cut -d' ' -f2 | \
sed 's/\(.*\)/IDENT(\1)/' | grep -v ELAST | \
sort
