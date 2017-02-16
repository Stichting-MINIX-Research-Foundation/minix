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

# Id: tests.sh,v 1.2 2010/06/21 02:31:45 marka Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

DIGOPTS="+noadd +nosea +nostat +nocmd +noauth +dnssec -p 5300"

ksk=ns1/`cat ns1/keyname`.key
kskpat=`awk '/DNSKEY/ { print $8 }' $ksk`
kskid=`sed 's/^Kexample\.+005+0*//' < ns1/keyname`
rkskid=`expr \( $kskid + 128 \) \% 65536`

echo "I:checking for KSK not yet published ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
# Note - this is looking for failure, hence the &&
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# 5s real, 55s virtual, P +20
sleep 4

echo "I:checking for KSK published but not yet active ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep 'RRSIG.*'" $kskid "'example\. ' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ] ; then echo "I:failed"; fi
status=`expr $status + $ret`

# 10s real, 2h15mn virtual, A +1h
sleep 5

echo "I:checking for KSK active ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null || ret=1
grep 'RRSIG.*'" $kskid "'example\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ] ; then echo "I:failed"; fi
status=`expr $status + $ret`

# 11s real, 6h7,m virtual, R +6h
sleep 1

echo "I:checking for KSK revoked ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null || ret=1
awk 'BEGIN { $noksk=1 } \
/DNSKEY/ { $5==385 && $noksk=0 } \
END { exit $noksk }' < dig.out.ns1.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep 'RRSIG.*'" $kskid "'example\. ' dig.out.ns1.test$n > /dev/null && ret=1
grep 'RRSIG.*'" $rkskid "'example\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ] ; then echo "I:failed"; fi
status=`expr $status + $ret`

# 13s real, 45h virtual, I +1d
sleep 2

echo "I:checking for KSK retired but not yet deleted ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ] ; then echo "I:failed"; fi
status=`expr $status + $ret`

# 17s real, 103d virtual, D +1mo
sleep 4

echo "I:checking for KSK deleted ($n)"
ret=0
$DIG $DIGOPTS -t dnskey example. @10.53.0.1 > dig.out.ns1.test$n || ret=1
# Note - this is looking for failure, hence the &&
tr -d ' ' < dig.out.ns1.test$n | grep $kskpat > /dev/null && ret=1
# Note - this is looking for failure, hence the &&
grep 'RRSIG.*'" $rkskid "'example\. ' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ] ; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
