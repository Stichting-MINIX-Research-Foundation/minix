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

for f in conf/good*.conf
do
	echo "I:checking '$f'"
	ret=0
	$CHECKCONF $f > /dev/null || ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

for f in conf/bad*.conf
do
	echo "I:checking '$f'"
	ret=0
	$CHECKCONF $f > /dev/null && ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

echo "I:checking that RSA big exponent keys can't be loaded"
ret=0
grep "out of range" ns2/signer.err > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that RSA big exponent signature can't validate"
ret=0
$DIG $DIGOPTS a.example @10.53.0.2 > dig.out.ns2 || ret=1
$DIG $DIGOPTS a.example @10.53.0.3 > dig.out.ns3 || ret=1
grep "status: NOERROR" dig.out.ns2 > /dev/null || ret=1
grep "status: SERVFAIL" dig.out.ns3 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
