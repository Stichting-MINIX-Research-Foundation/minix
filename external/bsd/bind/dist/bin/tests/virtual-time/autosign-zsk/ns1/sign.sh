#!/bin/sh -e
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

# Id: sign.sh,v 1.2 2010/06/21 02:31:46 marka Exp 

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data1
RANDFILE2=../random.data2

zone=example.
infile=example.db.in
zonefile=example.db

zskname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
kskname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -f KSK -n zone $zone`

cat $infile $zskname.key $kskname.key > $zonefile

$SIGNER -P -e +1000d -r $RANDFILE -o $zone $zonefile > /dev/null

# zsk, no -R
keyname=`$KEYGEN -q -r $RANDFILE2 -a RSASHA1 -b 768 -n zone \
	-P +20 -A +1h -I +1d -D +1mo $zone`

echo $keyname > keyname
