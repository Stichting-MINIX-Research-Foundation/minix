#!/bin/sh
#
# Copyright (C) 2010, 2012  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# Id: tests.sh,v 1.2 2010/06/17 05:38:06 marka Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd -p 5300"

echo "I:checking slave expiry"
ret=0
$DIG $DIGOPTS txt.example. txt @10.53.0.1 > dig.out.before || ret=1
echo "I:waiting for expiry (10s real, 6h virtual)"
sleep 10
$DIG $DIGOPTS txt.example. txt @10.53.0.1 > dig.out.after || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

ret=0
grep "status: NOERROR" dig.out.before > /dev/null || ret=1
if [ $ret -eq 1 ] ; then
    echo "I:failed (before)"; status=1
fi
ret=0
grep "status: SERVFAIL" dig.out.after > /dev/null || ret=1
if [ $ret -eq 1 ] ; then
    echo "I:failed (after)"; status=1
fi

echo "I:exit status: $status"
exit $status
