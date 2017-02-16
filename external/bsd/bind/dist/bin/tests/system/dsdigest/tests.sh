#!/bin/sh
#
# Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"

# Check the good. domain

echo "I:checking that validation with enabled digest types works"
ret=0
$DIG $DIGOPTS a.good. @10.53.0.3 a > dig.out.good || ret=1
grep "status: NOERROR" dig.out.good > /dev/null || ret=1
grep "flags:[^;]* ad[ ;]" dig.out.good > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check the bad. domain

echo "I:checking that validation with no supported digest types and must-be-secure results in SERVFAIL"
ret=0
$DIG $DIGOPTS a.bad. @10.53.0.3 a > dig.out.bad || ret=1
grep "SERVFAIL" dig.out.bad > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation with no supported digest algorithms results in insecure"
ret=0
$DIG $DIGOPTS bad. @10.53.0.4 ds > dig.out.ds || ret=1
grep "NOERROR" dig.out.ds > /dev/null || ret=1
grep "flags:[^;]* ad[ ;]" dig.out.ds > /dev/null || ret=1
$DIG $DIGOPTS a.bad. @10.53.0.4 a > dig.out.insecure || ret=1
grep "NOERROR" dig.out.insecure > /dev/null || ret=1
grep "flags:[^;]* ad[ ;]" dig.out.insecure > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
echo "I:exit status: $status"

exit $status
