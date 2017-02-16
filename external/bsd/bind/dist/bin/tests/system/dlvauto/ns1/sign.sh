#!/bin/sh -e
#
# Copyright (C) 2011, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

zone=dlv.isc.org
infile=dlv.isc.org.db.in
zonefile=dlv.isc.org.db

dlvkey=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`
cat $infile $dlvkey.key > $zonefile
$SIGNER -P -g -r $RANDFILE -o $zone $zonefile > /dev/null

zone=.
infile=root.db.in
zonefile=root.db

rootkey=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`
cat $infile $rootkey.key > $zonefile
$SIGNER -P -g -r $RANDFILE -o $zone $zonefile > /dev/null

# Create bind.keys file for the use of the resolving server
echo "managed-keys {" > bind.keys
cat $dlvkey.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
    "$dn" initial-key $flags $proto $alg "$key";
EOF
' >>  bind.keys
cat $rootkey.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
    "$dn" initial-key $flags $proto $alg "$key";
EOF
' >>  bind.keys
echo "};" >> bind.keys
