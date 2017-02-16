#!/bin/sh
#
# Copyright (C) 2010-2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.5 2011/01/11 23:47:12 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

for conf in conf/good*.conf
do
	n=`expr $n + 1`
	echo "I:checking that $conf is accepted ($n)"
	ret=0
	$CHECKCONF "$conf" || ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

for conf in conf/bad*.conf
do
	n=`expr $n + 1`
	echo "I:checking that $conf is rejected ($n)"
	ret=0
	$CHECKCONF "$conf" >/dev/null && ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

n=`expr $n + 1`
echo "I:trying an axfr that should be denied (NOTAUTH) ($n)"
ret=0
$DIG +tcp data.example. @10.53.0.2 axfr -p 5300 > dig.out.ns2.test$n || ret=1
grep "; Transfer failed." dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:non recursive query for a static-stub zone with server name should be rejected ($n)"
ret=0
 $DIG +tcp +norec data.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n \
 	|| ret=1
grep "REFUSED" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:non recursive query for a static-stub zone with server name should be rejected ($n)"
ret=0
$DIG +tcp +norec data.example.org. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n \
	|| ret=1
grep "REFUSED" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:allow-query ACL ($n)"
ret=0
$DIG +tcp +norec data.example. @10.53.0.2 txt -b 10.53.0.7 -p 5300 \
	> dig.out.ns2.test$n || ret=1
grep "REFUSED" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:look for static-stub zone data with recursion (should be found) ($n)"
ret=0
$DIG +tcp +noauth data.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
$PERL ../digcomp.pl knowngood.dig.out.rec dig.out.ns2.test$n || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking authoritative NS is ignored for delegation ($n)"
ret=0
# the auth server returns a different (and incorrect) NS for .example.
$DIG +tcp example. @10.53.0.2 ns -p 5300 > dig.out.ns2.test1.$n || ret=1
grep "ns4.example." dig.out.ns2.test1.$n > /dev/null || ret=1
# but static-stub configuration should still be used
$DIG +tcp data2.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test2.$n || ret=1
grep "2nd test data" dig.out.ns2.test2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking queries for a child zone of the static-stub zone ($n)"
ret=0
# prime the delegation to a child zone of the static-stub zone
$DIG +tcp data1.sub.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test1.$n || ret=1
grep "1st sub test data" dig.out.ns2.test1.$n > /dev/null || ret=1
# temporarily disable the the parent zone
sed 's/EXAMPLE_ZONE_PLACEHOLDER//' ns3/named.conf.in > ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 | sed 's/^/I:ns3 /'
# query the child zone again.  this should directly go to the child and
# succeed.
for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG +tcp data2.sub.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test2.$n || ret=1
	grep "2nd sub test data" dig.out.ns2.test2.$n > /dev/null && break
	sleep 1
done
grep "2nd sub test data" dig.out.ns2.test2.$n > /dev/null || ret=1
# re-enable the parent
sed 's/EXAMPLE_ZONE_PLACEHOLDER/zone "example" { type master; file "example.db.signed"; };/' ns3/named.conf.in > ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 | sed 's/^/I:ns3 /'
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking authoritative NS addresses are ignored for delegation ($n)"
ret=0
# the auth server returns a different (and incorrect) A/AAA RR for .example.
$DIG +tcp example. @10.53.0.2 a -p 5300 > dig.out.ns2.test1.$n || ret=1
grep "10.53.0.4" dig.out.ns2.test1.$n > /dev/null || ret=1
$DIG +tcp example. @10.53.0.2 aaaa -p 5300 > dig.out.ns2.test2.$n || ret=1
grep "::1" dig.out.ns2.test2.$n > /dev/null || ret=1
# reload the server.  this will flush the ADB.
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
# ask another RR that would require delegation.  static-stub configuration
# should still be used instead of the authoritative A/AAAA cached above.
$DIG +tcp data3.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test3.$n || ret=1
grep "3rd test data" dig.out.ns2.test3.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# the authoritative server of the query domain (example.com) is the apex
# name of the static-stub zone (example).  in this case the static-stub
# configuration must be ignored and cached information must be used.
n=`expr $n + 1`
echo "I:checking NS of static-stub is ignored when referenced from other domain ($n)"
ret=0
$DIG +tcp data.example.com. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
grep "example com data" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# check server-names
n=`expr $n + 1`
echo "I:checking static-stub with a server-name ($n)"
ret=0
$DIG +tcp data.example.org. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
grep "example org data" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
# Note: for a short term workaround we use ::1, assuming it's configured and
# usable for our tests.  We should eventually use the test ULA and available
# checks introduced in change 2916.
if $PERL ../testsock6.pl ::1 2> /dev/null
then
    echo "I:checking IPv6 static-stub address ($n)"
    ret=0
    $DIG +tcp data.example.info. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
    grep "example info data" dig.out.ns2.test$n > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
else
    echo "I:SKIPPED: checking IPv6 static-stub address ($n)"
fi

n=`expr $n + 1`
echo "I:look for static-stub zone data with DNSSEC validation ($n)"
ret=0
$DIG +tcp +dnssec data4.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
grep "ad; QUERY" dig.out.ns2.test$n > /dev/null || ret=1
grep "4th test data" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:look for a child of static-stub zone data with DNSSEC validation ($n)"
ret=0
$DIG +tcp +dnssec data3.sub.example. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
grep "ad; QUERY" dig.out.ns2.test$n > /dev/null || ret=1
grep "3rd sub test data" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# reload with a different name server: exisitng zone shouldn't be reused.
n=`expr $n + 1`
echo "I:checking server reload with a different static-stub config ($n)"
ret=0
sed 's/SERVER_CONFIG_PLACEHOLDER/server-addresses { 10.53.0.4; };/' ns2/named.conf.in > ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
$DIG +tcp data2.example.org. @10.53.0.2 txt -p 5300 > dig.out.ns2.test$n || ret=1
grep "2nd example org data" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
