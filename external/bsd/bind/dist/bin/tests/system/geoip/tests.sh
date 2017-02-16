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

status=0
n=0

rm -f dig.out.*

DIGOPTS="+tcp +short -p 5300 @10.53.0.2"
DIGOPTS6="+tcp +short -p 5300 @fd92:7065:b8e:ffff::2"

n=`expr $n + 1`
echo "I:checking GeoIP country database by code ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named2.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP country database by three-letter code ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named3.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP country database by name ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named4.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP region code, no specified database ($n)"
ret=0
lret=0
# skipping 2 on purpose here; it has the same region code as 1
for i in 1 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named5.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP region database by region name and country code ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named6.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

if $TESTSOCK6 fd92:7065:b8e:ffff::3
then
  n=`expr $n + 1`
  echo "I:checking GeoIP city database by city name using IPv6 ($n)"
  ret=0
  $DIG +tcp +short -p 5300 @fd92:7065:b8e:ffff::1 -6 txt example -b fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
  [ $ret -eq 0 ] || echo "I:failed"
  status=`expr $status + $ret`
else
  echo "I:IPv6 unavailable; skipping"
fi

n=`expr $n + 1`
echo "I:checking GeoIP city database by city name ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named7.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP isp database ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named8.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP org database ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named9.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP asnum database ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named10.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP asnum database - ASNNNN only ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named11.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP domain database ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named12.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP netspeed database ($n)"
ret=0
lret=0
for i in 1 2 3 4; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named13.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP blackhole ACL ($n)"
ret=0
$DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n || ret=1
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 status 2>&1 > rndc.out.ns2.test$n || ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:reloading server"
cp -f ns2/named14.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3

n=`expr $n + 1`
echo "I:checking GeoIP country database by code (using nested ACLs) ($n)"
ret=0
lret=0
for i in 1 2 3 4 5 6 7; do
    $DIG $DIGOPTS txt example -b 10.53.0.$i > dig.out.ns2.test$n.$i || lret=1
    j=`cat dig.out.ns2.test$n.$i | tr -d '"'`
    [ "$i" = "$j" ] || lret=1
    [ $lret -eq 1 ] && break
done
[ $lret -eq 1 ] && ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:reloading server with different geoip-directory ($n)"
ret=0
cp -f ns2/named15.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 3
awk '/using "..\/data2" as GeoIP directory/ {m=1} ; { if (m>0) { print } }' ns2/named.run | grep "GeoIP City .* DB not available" > /dev/null || ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking GeoIP v4/v6 when only IPv6 database is available ($n)"
ret=0
$DIG $DIGOPTS -4 txt example -b 10.53.0.2 > dig.out.ns2.test$n.1 || ret=1
j=`cat dig.out.ns2.test$n.1 | tr -d '"'`
[ "$j" = "bogus" ] || ret=1
if $TESTSOCK6 fd92:7065:b8e:ffff::2; then
    $DIG $DIGOPTS6 txt example -b fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n.2 || ret=1
    j=`cat dig.out.ns2.test$n.2 | tr -d '"'`
    [ "$j" = "2" ] || ret=1
fi
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
