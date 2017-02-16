#!/bin/sh
#
# Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

DIGOPTS="-p 5300"

status=0
n=0

n=`expr $n + 1`
echo "I: attempt excessive-depth lookup ($n)"
ret=0
echo "1000" > ans2/ans.limit
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: SERVFAIL" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -eq 26 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: attempt permissible lookup ($n)"
ret=0
echo "12" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -eq 49 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:reset max-recursion-depth"
cp ns3/named2.conf ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reconfig 2>&1 | sed 's/^/I:ns1 /'
sleep 2

n=`expr $n + 1`
echo "I: attempt excessive-depth lookup ($n)"
ret=0
echo "12" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: SERVFAIL" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -eq 12 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: attempt permissible lookup ($n)"
ret=0
echo "5" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -eq 21 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:reset max-recursion-depth"
cp ns3/named3.conf ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reconfig 2>&1 | sed 's/^/I:ns1 /'
sleep 2

n=`expr $n + 1`
echo "I: attempt excessive-queries lookup ($n)"
ret=0
echo "13" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: SERVFAIL" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -le 50 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: attempt permissible lookup ($n)"
ret=0
echo "12" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -le 50 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:reset max-recursion-queries"
cp ns3/named4.conf ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reconfig 2>&1 | sed 's/^/I:ns1 /'
sleep 2

n=`expr $n + 1`
echo "I: attempt excessive-queries lookup ($n)"
ret=0
echo "10" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: SERVFAIL" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -le 40 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: attempt permissible lookup ($n)"
ret=0
echo "9" > ans2/ans.limit
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS @10.53.0.2 reset > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.3 indirect.example.org > dig.out.1.test$n || ret=1
grep "status: NOERROR" dig.out.1.test$n > /dev/null || ret=1
$DIG $DIGOPTS +short @10.53.0.2 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -le 40 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: attempting NS explosion ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 flush 2>&1 | sed 's/^/I:ns1 /'
$DIG $DIGOPTS +short @10.53.0.3 ns1.1.example.net > dig.out.1.test$n || ret=1
sleep 2
$DIG $DIGOPTS +short @10.53.0.4 count txt > dig.out.2.test$n || ret=1
eval count=`cat dig.out.2.test$n`
[ $count -lt 50 ] || ret=1
$DIG $DIGOPTS +short @10.53.0.7 count txt > dig.out.3.test$n || ret=1
eval count=`cat dig.out.3.test$n`
[ $count -lt 50 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
