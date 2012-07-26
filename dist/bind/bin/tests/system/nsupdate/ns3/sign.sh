#!/bin/sh -e
#
# Copyright (C) 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: sign.sh,v 1.2.26.2 2011-02-28 01:20:01 tbox Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=nsec3param.test.
infile=nsec3param.test.db.in
zonefile=nsec3param.test.db

keyname1=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone -f KSK $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -3 - -H 1 -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null

zone=dnskey.test.
infile=dnskey.test.db.in
zonefile=dnskey.test.db

keyname1=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null
