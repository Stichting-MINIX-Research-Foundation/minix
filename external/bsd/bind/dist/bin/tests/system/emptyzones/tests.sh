#!/bin/sh
#
# Copyright (C) 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

n=`expr $n + 1`
echo "I:check that switching to automatic empty zones works ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload > /dev/null || ret=1
sleep 5
cp ns1/named2.conf ns1/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload > /dev/null || ret=1
sleep 5
$DIG +vc version.bind txt ch @10.53.0.1 -p 5300 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check that allow-transfer { none; } works ($n)"
ret=0
$DIG axfr 10.in-addr.arpa @10.53.0.1 -p 5300 +all > dig.out.test$n || ret=1
grep "status: REFUSED" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

exit $status
