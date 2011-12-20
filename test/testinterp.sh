#!/bin/sh

# See if the system can handle an unexpected whitespace-only interpreter line

echo -n "Test interpreter "

set -e
TMPSH=tst$$.sh
rm -f $TMPSH
echo '#!   ' >$TMPSH
chmod 755 $TMPSH
./$TMPSH || true
rm -f $TMPSH
echo "ok"
