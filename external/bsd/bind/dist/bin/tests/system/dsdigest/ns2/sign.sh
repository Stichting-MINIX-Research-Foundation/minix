#!/bin/sh -e
#
# Copyright (C) 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

zone1=good.
infile1=good.db.in
zonefile1=good.db
zone2=bad.
infile2=bad.db.in
zonefile2=bad.db

keyname11=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone1`
keyname12=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 2048 -n zone -f KSK $zone1`
keyname21=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone2`
keyname22=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 2048 -n zone -f KSK $zone2`

cat $infile1 $keyname11.key $keyname12.key >$zonefile1
cat $infile2 $keyname21.key $keyname22.key >$zonefile2

$SIGNER -P -g -r $RANDFILE -o $zone1 $zonefile1 > /dev/null
$SIGNER -P -g -r $RANDFILE -o $zone2 $zonefile2 > /dev/null

$DSFROMKEY -a SHA-256 $keyname12 > dsset-$zone1
$DSFROMKEY -a SHA-256 $keyname22 > dsset-$zone2

supported=`cat ../supported`
case "$supported" in
    gost) algo=GOST ;;
    *) algo=SHA-384 ;;
esac

$DSFROMKEY -a $algo $keyname12 >> dsset-$zone1
$DSFROMKEY -a $algo $keyname22 > dsset-$zone2
