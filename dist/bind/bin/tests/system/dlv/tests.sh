#!/bin/sh
#
# Copyright (C) 2004, 2007, 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.6.120.2 2011-05-26 23:47:05 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"

echo "I:checking that DNSKEY reference by DLV validates as secure ($n)"
ret=0
$DIG $DIGOPTS child1.utld dnskey @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that child DNSKEY reference by DLV validates as secure ($n)"
ret=0
$DIG $DIGOPTS grand.child1.utld dnskey @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that SOA reference by DLV in a DRUZ with DS validates as secure ($n)"
ret=0
$DIG $DIGOPTS child1.druz soa @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that child SOA reference by DLV in a DRUZ with DS validates as secure ($n)"
ret=0
$DIG $DIGOPTS grand.child1.druz soa @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
