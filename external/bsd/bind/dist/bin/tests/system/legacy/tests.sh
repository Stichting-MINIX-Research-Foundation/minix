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

# Id

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

n=`expr $n + 1`
echo "I:checking drop edns server setup ($n)"
ret=0
$DIG +edns @10.53.0.2 -p 5300 dropedns soa > dig.out.1.test$n
grep "connection timed out; no servers could be reached" dig.out.1.test$n > /dev/null || ret=1
$DIG +noedns @10.53.0.2 -p 5300 dropedns soa > dig.out.2.test$n || ret=1
grep "status: NOERROR" dig.out.2.test$n > /dev/null || ret=1
grep "EDNS: version:" dig.out.2.test$n > /dev/null && ret=1
$DIG +noedns +tcp @10.53.0.2 -p 5300 dropedns soa > dig.out.3.test$n || ret=1
grep "status: NOERROR" dig.out.3.test$n > /dev/null || ret=1
grep "EDNS: version:" dig.out.3.test$n > /dev/null && ret=1
$DIG +edns +tcp @10.53.0.2 -p 5300 dropedns soa > dig.out.4.test$n
grep "connection timed out; no servers could be reached" dig.out.4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to drop edns server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 dropedns soa > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking drop edns + no tcp server setup ($n)"
ret=0
$DIG +edns @10.53.0.3 -p 5300 dropedns-notcp soa > dig.out.1.test$n
grep "connection timed out; no servers could be reached" dig.out.1.test$n > /dev/null || ret=1
$DIG +noedns +tcp  @10.53.0.3 -p 5300 dropedns-notcp soa > dig.out.2.test$n
grep "connection timed out; no servers could be reached" dig.out.2.test$n > /dev/null
$DIG +noedns @10.53.0.3 -p 5300 dropedns-notcp soa > dig.out.3.test$n || ret=1
grep "status: NOERROR" dig.out.3.test$n > /dev/null || ret=1
grep "EDNS: version:" dig.out.3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to drop edns + no tcp server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 dropedns-notcp soa > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking plain dns server setup ($n)"
ret=0
$DIG +edns @10.53.0.4 -p 5300 plain soa > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
grep "EDNS: version:" dig.out.1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to plain dns server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 plain soa > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking plain dns + no tcp server setup ($n)"
ret=0
$DIG +edns @10.53.0.5 -p 5300 plain-notcp soa > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
grep "EDNS: version:" dig.out.1.test$n > /dev/null && ret=1
$DIG +edns +tcp @10.53.0.5 -p 5300 plain-notcp soa > dig.out.2.test$n
grep "connection timed out; no servers could be reached" dig.out.2.test$n > /dev/null
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to plain dns + no tcp server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 plain-notcp soa > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I:checking edns 512 server setup ($n)"
ret=0
$DIG +edns @10.53.0.6 -p 5300 edns512 soa > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG +edns +tcp  @10.53.0.6 -p 5300 edns512 soa > dig.out.2.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG +edns @10.53.0.6 -p 5300 txt500.edns512 txt > dig.out.3.test$n
grep "connection timed out; no servers could be reached" dig.out.3.test$n > /dev/null
$DIG +edns +bufsize=512 +ignor @10.53.0.6 -p 5300 txt500.edns512 txt > dig.out.4.test$n
grep "status: NOERROR" dig.out.4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to edns 512 server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 txt500.edns512 txt > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking edns 512 + no tcp server setup ($n)"
ret=0
$DIG +noedns @10.53.0.7 -p 5300 edns512-notcp soa > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG +noedns +tcp  @10.53.0.7 -p 5300 edns512-notcp soa > dig.out.2.test$n
grep "connection timed out; no servers could be reached" dig.out.2.test$n > /dev/null
$DIG +edns @10.53.0.7 -p 5300 edns512-notcp soa > dig.out.3.test$n
grep "connection timed out; no servers could be reached" dig.out.3.test$n > /dev/null
$DIG +edns +bufsize=512 +ignor @10.53.0.7 -p 5300 edns512-notcp soa > dig.out.4.test$n
grep "status: NOERROR" dig.out.4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking recursive lookup to edns 512 + no tcp server succeeds ($n)"
ret=0
$DIG +tcp  @10.53.0.1 -p 5300 edns512-notcp soa > dig.out.test$n || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if $SHELL ../testcrypto.sh > /dev/null 2>&1
then
    $PERL $SYSTEMTESTTOP/stop.pl . ns1

    cp -f ns1/named2.conf ns1/named.conf

    $PERL $SYSTEMTESTTOP/start.pl --noclean --restart . ns1

    n=`expr $n + 1`
    echo "I:checking recursive lookup to edns 512 + no tcp + trust anchor fails ($n)"
    ret=0
    $DIG +tcp  @10.53.0.1 -p 5300 edns512-notcp soa > dig.out.test$n
    grep "status: SERVFAIL" dig.out.test$n > /dev/null ||
        grep "connection timed out;" dig.out.test$n > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
else
    echo "I:skipping checking recursive lookup to edns 512 + no tcp + trust anchor fails as crypto not enabled"
fi 


echo "I:exit status: $status"
exit $status
