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

infile=ns1/example.db.in

/bin/echo -n ${HSMPIN:-1234}> pin
PWD=`pwd`

zone=rsa.example
zonefile=ns1/rsa.example.db

$PK11GEN -a RSA -b 1024 -l robie-rsa-zsk1 -i 01
$PK11GEN -a RSA -b 1024 -l robie-rsa-zsk2 -i 02
$PK11GEN -a RSA -b 2048 -l robie-rsa-ksk

rsazsk1=`$KEYFRLAB -a RSASHA1 \
        -l "robie-rsa-zsk1" rsa.example`
rsazsk2=`$KEYFRLAB -a RSASHA1 \
        -l "robie-rsa-zsk2" rsa.example`
rsaksk=`$KEYFRLAB -a RSASHA1 -f ksk \
        -l "robie-rsa-ksk" rsa.example`

cat $infile $rsazsk1.key $rsaksk.key > $zonefile
$SIGNER -a -P -g -r $RANDFILE -o $zone $zonefile \
        > /dev/null 2> signer.err || cat signer.err
cp $rsazsk2.key ns1/rsa.key
mv Krsa* ns1

rm -f signer.err
