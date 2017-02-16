#!/bin/sh
#
# Copyright (C) 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

DIG="./dig.sh"
chmod +x $DIG

CHECKDS="$CHECKDS -d $DIG -D $DSFROMKEY"

status=0
n=1

echo "I:checking for correct DS, looking up key via 'dig' ($n)"
ret=0
$CHECKDS ok.example > checkds.out.$n 2>&1 || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for correct DS, obtaining key from file ($n)"
ret=0
$CHECKDS -f ok.example.dnskey.db ok.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for correct DLV, looking up key via 'dig' ($n)"
ret=0
$CHECKDS -l dlv.example ok.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for correct DLV, obtaining key from file ($n)"
ret=0
$CHECKDS -l dlv.example -f ok.example.dnskey.db ok.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for incorrect DS, lowronging up key via 'dig' ($n)"
ret=0
$CHECKDS wrong.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for incorrect DS, obtaining key from file ($n)"
ret=0
$CHECKDS -f wrong.example.dnskey.db wrong.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for incorrect DLV, lowronging up key via 'dig' ($n)"
ret=0
$CHECKDS -l dlv.example wrong.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for incorrect DLV, obtaining key from file ($n)"
ret=0
$CHECKDS -l dlv.example -f wrong.example.dnskey.db wrong.example > checkds.out.$n || ret=1
grep 'SHA-1' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


echo "I:checking for partially missing DS, looking up key via 'dig' ($n)"
ret=0
$CHECKDS missing.example > checkds.out.$n || ret=1
grep 'SHA-1.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-1.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for partially missing DS, obtaining key from file ($n)"
ret=0
$CHECKDS -f missing.example.dnskey.db missing.example > checkds.out.$n || ret=1
grep 'SHA-1.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-1.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for partially missing DLV, looking up key via 'dig' ($n)"
ret=0
$CHECKDS -l dlv.example missing.example > checkds.out.$n || ret=1
grep 'SHA-1.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-1.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for partially missing DLV, obtaining key from file ($n)"
ret=0
$CHECKDS -l dlv.example -f missing.example.dnskey.db missing.example > checkds.out.$n || ret=1
grep 'SHA-1.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*found' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-1.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
grep 'SHA-256.*missing' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for entirely missing DS, looking up key via 'dig' ($n)"
ret=0
$CHECKDS none.example > checkds.out.$n && ret=1
grep 'No DS' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for entirely missing DS, obtaining key from file ($n)"
ret=0
$CHECKDS -f none.example.dnskey.db none.example > checkds.out.$n && ret=1
grep 'No DS' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for entirely missing DLV, looking up key via 'dig' ($n)"
ret=0
$CHECKDS -l dlv.example none.example > checkds.out.$n && ret=1
grep 'No DLV' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for entirely missing DLV, obtaining key from file ($n)"
ret=0
$CHECKDS -l dlv.example -f none.example.dnskey.db none.example > checkds.out.$n && ret=1
grep 'No DLV' checkds.out.$n > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ $status = 0 ]; then $SHELL clean.sh; fi
echo "I:exit status: $status"
exit $status
