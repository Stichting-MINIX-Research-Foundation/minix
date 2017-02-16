#!/bin/sh
#
# Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.3 2011/03/01 23:48:05 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"
RNDCCMD="$RNDC -s 10.53.0.1 -p 9953 -c ../common/rndc.conf"

# Check the example. domain

echo "I:checking pre reload zone ($n)"
ret=0
$DIG $DIGOPTS soa database. @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "hostmaster\.isc\.org" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

cp ns1/named.conf2 ns1/named.conf
$RNDCCMD reload 2>&1 >/dev/null

echo "I:checking post reload zone ($n)"
ret=1
try=0
while test $try -lt 6
do
	sleep 1
	ret=0
	$DIG $DIGOPTS soa database. @10.53.0.1 > dig.out.ns1.test$n || ret=1
	grep "marka\.isc\.org" dig.out.ns1.test$n > /dev/null || ret=1
	try=`expr $try + 1`
	test $ret -eq 0 && break
done
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
