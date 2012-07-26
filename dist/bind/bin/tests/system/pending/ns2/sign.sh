#!/bin/sh -e
#
# Copyright (C) 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: sign.sh,v 1.7 2010-01-18 19:19:31 each Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

for domain in example example.com; do
	zone=${domain}.
	infile=${domain}.db.in
	zonefile=${domain}.db

	keyname1=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`
	keyname2=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -f KSK -n zone $zone`

	cat $infile $keyname1.key $keyname2.key > $zonefile

	$SIGNER -3 bebe -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
done

# remove "removed" record from example.com, causing the server to
# send an apparently-invalid NXDOMAIN
sed '/^removed/d' example.com.db.signed > example.com.db.new
rm -f example.com.db.signed
mv example.com.db.new example.com.db.signed
