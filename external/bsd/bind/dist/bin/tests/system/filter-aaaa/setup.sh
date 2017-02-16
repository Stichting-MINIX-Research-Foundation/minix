#!/bin/sh
#
# Copyright (C) 2010, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

$SHELL clean.sh

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

cp ns1/named1.conf ns1/named.conf
cp ns2/named1.conf ns2/named.conf
cp ns3/named1.conf ns3/named.conf
cp ns4/named1.conf ns4/named.conf

if $SHELL ../testcrypto.sh -q
then
	(cd ns1 && $SHELL -e sign.sh)
	(cd ns4 && $SHELL -e sign.sh)
else
	echo "I:using pre-signed zones"
	cp -f ns1/signed.db.presigned ns1/signed.db.signed
	cp -f ns4/signed.db.presigned ns4/signed.db.signed
fi
