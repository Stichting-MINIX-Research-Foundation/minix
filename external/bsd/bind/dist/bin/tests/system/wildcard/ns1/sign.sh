#!/bin/sh
#
# Copyright (C) 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: sign.sh,v 1.1.2.2 2010/06/01 06:38:47 marka Exp 

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

dssets=

zone=dlv.
infile=dlv.db.in
zonefile=dlv.db
outfile=dlv.db.signed
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key > $zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=nsec.
infile=nsec.db.in
zonefile=nsec.db
outfile=nsec.db.signed
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key > $zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=private.nsec.
infile=private.nsec.db.in
zonefile=private.nsec.db
outfile=private.nsec.db.signed

keyname1=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key > $zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

grep -v '^;' $keyname2.key | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > private.nsec.conf

zone=nsec3.
infile=nsec3.db.in
zonefile=nsec3.db
outfile=nsec3.db.signed
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key > $zonefile

$SIGNER -r $RANDFILE -3 - -H 10 -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=private.nsec3.
infile=private.nsec3.db.in
zonefile=private.nsec3.db
outfile=private.nsec3.db.signed

keyname1=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key > $zonefile

$SIGNER -r $RANDFILE -3 - -H 10 -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

grep -v '^;' $keyname2.key | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > private.nsec3.conf

zone=.
infile=root.db.in
zonefile=root.db
outfile=root.db.signed

keyname1=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key $dssets >$zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
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
