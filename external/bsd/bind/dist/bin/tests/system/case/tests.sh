#!/bin/sh
#
# Copyright (C) 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
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

DIGOPTS="+tcp +nosea +nostat +noquest +nocomm +nocmd"

status=0
n=0

n=`expr $n + 1`
echo "I:waiting for zone transfer to complete ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9
do
	$DIG $DIGOPTS soa example. @10.53.0.2 -p 5300 > dig.ns2.test$n
	grep SOA dig.ns2.test$n > /dev/null && break
done

n=`expr $n + 1`
echo "I:testing case preserving responses - no acl ($n)"
ret=0
$DIG $DIGOPTS mx example. @10.53.0.1 -p 5300 > dig.ns1.test$n
grep "0.mail.eXaMpLe" dig.ns1.test$n > /dev/null || ret=1
grep "mAiL.example" dig.ns1.test$n > /dev/null || ret=1
test $ret -eq 0 || echo "I:failed"
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:testing no-case-compress acl '{ 10.53.0.2; }' ($n)"
ret=0

# check that we preserve zone case for non-matching query (10.53.0.1)
$DIG $DIGOPTS mx example. -b 10.53.0.1 @10.53.0.1 -p 5300 > dig.ns1.test$n
grep "0.mail.eXaMpLe" dig.ns1.test$n > /dev/null || ret=1
grep "mAiL.example" dig.ns1.test$n > /dev/null || ret=1

# check that we don't preserve zone case for match (10.53.0.2)
$DIG $DIGOPTS mx example. -b 10.53.0.2 @10.53.0.2 -p 5300 > dig.ns2.test$n
grep "0.mail.example" dig.ns2.test$n > /dev/null || ret=1
grep "mail.example" dig.ns2.test$n > /dev/null || ret=1

test $ret -eq 0 || echo "I:failed"
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
