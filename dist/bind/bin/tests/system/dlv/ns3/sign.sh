#!/bin/sh
#
# Copyright (C) 2004, 2007, 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: sign.sh,v 1.9.120.2 2011-05-26 23:47:05 tbox Exp $

(cd ../ns6 && sh -e ./sign.sh)

echo "I:dlv/ns3/sign.sh"

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data
dlvzone=dlv.utld.
dlvsets=
dssets=

zone=child1.utld.
infile=child.db.in
zonefile=child1.utld.db
outfile=child1.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child3.utld.
infile=child.db.in
zonefile=child3.utld.db
outfile=child3.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child4.utld.
infile=child.db.in
zonefile=child4.utld.db
outfile=child4.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child5.utld.
infile=child.db.in
zonefile=child5.utld.db
outfile=child5.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child7.utld.
infile=child.db.in
zonefile=child7.utld.db
outfile=child7.signed

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child8.utld.
infile=child.db.in
zonefile=child8.utld.db
outfile=child8.signed

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child9.utld.
infile=child.db.in
zonefile=child9.utld.db
outfile=child9.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=child10.utld.
infile=child.db.in
zonefile=child10.utld.db
outfile=child10.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=child1.druz.
infile=child.db.in
zonefile=child1.druz.db
outfile=child1.druz.signed
dlvsets="$dlvsets dlvset-$zone"
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child3.druz.
infile=child.db.in
zonefile=child3.druz.db
outfile=child3.druz.signed
dlvsets="$dlvsets dlvset-$zone"
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child4.druz.
infile=child.db.in
zonefile=child4.druz.db
outfile=child4.druz.signed
dlvsets="$dlvsets dlvset-$zone"
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child5.druz.
infile=child.db.in
zonefile=child5.druz.db
outfile=child5.druz.signed
dlvsets="$dlvsets dlvset-$zone"
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child7.druz.
infile=child.db.in
zonefile=child7.druz.db
outfile=child7.druz.signed
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key ../ns6/dsset-grand.$zone >$zonefile

$SIGNER -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child8.druz.
infile=child.db.in
zonefile=child8.druz.db
outfile=child8.druz.signed

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=child9.druz.
infile=child.db.in
zonefile=child9.druz.db
outfile=child9.druz.signed
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"

zone=child10.druz.
infile=child.db.in
zonefile=child10.druz.db
outfile=child10.druz.signed
dlvsets="$dlvsets dlvset-$zone"
dssets="$dssets dsset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err
echo "I: signed $zone"


zone=dlv.utld.
infile=dlv.db.in
zonefile=dlv.utld.db
outfile=dlv.signed

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $dlvsets $keyname1.key $keyname2.key >$zonefile

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
' > trusted-dlv.conf
cp trusted-dlv.conf ../ns5

cp $dssets ../ns2
