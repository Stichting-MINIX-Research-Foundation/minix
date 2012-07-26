#!/bin/sh -e
#
# Copyright (C) 2004, 2006-2011  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2003  Internet Software Consortium.
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

# $Id: sign.sh,v 1.36.14.1 2011-05-03 16:09:23 marka Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=.
infile=root.db.in
zonefile=root.db

(cd ../ns2 && sh sign.sh )

cp ../ns2/dsset-example. .
cp ../ns2/dsset-dlv. .
grep "8 [12] " ../ns2/dsset-algroll. > dsset-algroll.

keyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key > $zonefile

$SIGNER -P -g -r $RANDFILE -o $zone $zonefile > /dev/null

# Configure the resolving server with a trusted key.
cat $keyname.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf

# ...or with a managed key.
cat $keyname.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
managed-keys {
    "$dn" initial-key $flags $proto $alg "$key";
};
EOF
' > managed.conf
cp trusted.conf ../ns2/trusted.conf
cp trusted.conf ../ns3/trusted.conf
cp trusted.conf ../ns4/trusted.conf
cp trusted.conf ../ns6/trusted.conf
cp trusted.conf ../ns7/trusted.conf
cp managed.conf ../ns4/managed.conf
#
#  Save keyid for managed key id test.
#
keyid=`expr $keyname : 'K.+001+\(.*\)'`
keyid=`expr $keyid + 0`
echo "$keyid" > managed.key.id
