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

# $Id: keygen.sh,v 1.6 2010-01-18 23:48:40 tbox Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=.
zonefile=root.db
infile=root.db.in

(cd ../ns2 && sh keygen.sh )

cat $infile ../ns2/dsset-example. > $zonefile

zskact=`$KEYGEN -3 -q -r $RANDFILE $zone`
zskvanish=`$KEYGEN -3 -q -r $RANDFILE $zone`
zskdel=`$KEYGEN -3 -q -r $RANDFILE -D now $zone`
zskinact=`$KEYGEN -3 -q -r $RANDFILE -I now $zone`
zskunpub=`$KEYGEN -3 -q -r $RANDFILE -G $zone`
zsksby=`$KEYGEN -3 -q -r $RANDFILE -A none $zone`
zsknopriv=`$KEYGEN -3 -q -r $RANDFILE $zone`
rm $zsknopriv.private

ksksby=`$KEYGEN -3 -q -r $RANDFILE -P now -A now+15s -fk $zone`
kskrev=`$KEYGEN -3 -q -r $RANDFILE -R now+15s -fk $zone`

cat $ksksby.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf
cp trusted.conf ../ns2/trusted.conf
cp trusted.conf ../ns3/trusted.conf
cp trusted.conf ../ns4/trusted.conf

cat $kskrev.key | grep -v '^; ' | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf
cp trusted.conf ../ns5/trusted.conf

echo $zskact > ../active.key
echo $zskvanish > ../vanishing.key
echo $zskdel > ../del.key
echo $zskinact > ../inact.key
echo $zskunpub > ../unpub.key
echo $zsknopriv > ../nopriv.key
echo $zsksby > ../standby.key
echo $kskrev > ../rev.key
