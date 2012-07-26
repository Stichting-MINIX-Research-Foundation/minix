#!/bin/sh
#
# Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: sign.sh,v 1.2.2.3 2011-05-26 23:47:05 tbox Exp $

(cd ../ns2 && sh -e ./sign.sh || exit 1)

echo "I:dlv/ns1/sign.sh"

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=.
infile=root.db.in
zonefile=root.db
outfile=root.signed

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -g -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err

echo "I: signed $zone"

grep -v '^;' $keyname2.key | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf
cp trusted.conf ../ns5

