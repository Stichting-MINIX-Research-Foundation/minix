#!/bin/sh
# to really generate all files you need to run "make distcheck" in a
# object tree, but this will do if you have all parts of the required
# tool-chain installed
set -e
autoreconf -f -i || { echo "autoreconf failed: $?"; exit 1; }
find . \( -name '*-private.h' -o -name '*-protos.h' \) | xargs rm -f
perl -MJSON -e 'print foo;' || \
    { echo "you must install JSON perl module (cpan install JSON)"; exit 1; }
