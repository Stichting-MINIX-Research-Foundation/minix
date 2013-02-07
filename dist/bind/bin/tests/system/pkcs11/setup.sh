#!/bin/sh
#
# Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: setup.sh,v 1.3 2010-06-08 23:50:24 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=random.data

zone=example
infile=ns1/example.db.in
zonefile=ns1/example.db

$PK11GEN -b 1024 -l robie-zsk1 -i 01
$PK11GEN -b 1024 -l robie-zsk2 -i 02
$PK11GEN -b 2048 -l robie-ksk

zsk1=`$KEYFRLAB -a RSASHA1 -l robie-zsk1 example`
zsk2=`$KEYFRLAB -a RSASHA1 -l robie-zsk2 example`
ksk=`$KEYFRLAB -a RSASHA1 -f ksk -l robie-ksk example`

cat $infile $zsk1.key $ksk.key > $zonefile
$SIGNER -a -P -g -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

cp $zsk2.key ns1/key
mv Kexample* ns1
