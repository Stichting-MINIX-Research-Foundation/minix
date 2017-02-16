#!/bin/sh
#
# Copyright (C) 2004, 2007, 2009-2012, 2014  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

#
# jnl and database files MUST be removed before we start
#

rm -f ns1/*.jnl ns1/example.db ns2/*.jnl ns2/example.bk
rm -f ns2/update.bk ns2/update.alt.bk
rm -f ns3/example.db.jnl

cp -f ns1/example1.db ns1/example.db
sed 's/example.nil/other.nil/g' ns1/example1.db > ns1/other.db
sed 's/example.nil/unixtime.nil/g' ns1/example1.db > ns1/unixtime.db
sed 's/example.nil/keytests.nil/g' ns1/example1.db > ns1/keytests.db
cp -f ns3/example.db.in ns3/example.db

# update_test.pl has its own zone file because it
# requires a specific NS record set.
cat <<\EOF >ns1/update.db
$ORIGIN .
$TTL 300        ; 5 minutes
update.nil              IN SOA  ns1.example.nil. hostmaster.example.nil. (
                                1          ; serial
                                2000       ; refresh (2000 seconds)
                                2000       ; retry (2000 seconds)
                                1814400    ; expire (3 weeks)
                                3600       ; minimum (1 hour)
                                )
update.nil.             NS      ns1.update.nil.
ns1.update.nil.         A       10.53.0.2
ns2.update.nil.		AAAA	::1
EOF

$DDNSCONFGEN -q -r $RANDFILE -z example.nil > ns1/ddns.key

$DDNSCONFGEN -q -r $RANDFILE -a hmac-md5 -k md5-key -z keytests.nil > ns1/md5.key
$DDNSCONFGEN -q -r $RANDFILE -a hmac-sha1 -k sha1-key -z keytests.nil > ns1/sha1.key
$DDNSCONFGEN -q -r $RANDFILE -a hmac-sha224 -k sha224-key -z keytests.nil > ns1/sha224.key
$DDNSCONFGEN -q -r $RANDFILE -a hmac-sha256 -k sha256-key -z keytests.nil > ns1/sha256.key
$DDNSCONFGEN -q -r $RANDFILE -a hmac-sha384 -k sha384-key -z keytests.nil > ns1/sha384.key
$DDNSCONFGEN -q -r $RANDFILE -a hmac-sha512 -k sha512-key -z keytests.nil > ns1/sha512.key

(cd ns3; $SHELL -e sign.sh)
