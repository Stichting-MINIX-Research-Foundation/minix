#!/bin/sh -e
#
# Copyright (C) 2009-2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: keygen.sh,v 1.15 2012/02/06 23:46:46 tbox Exp 

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

dumpit () {
	echo "D:${debug}: dumping ${1}"
	cat "${1}" | sed 's/^/D:/'
}

setup () {
	echo "I:setting up zone: $1"
	debug="$1"
	zone="$1"
	zonefile="${zone}.db"
	infile="${zonefile}.in"
	n=`expr ${n:-0} + 1`
}

setup secure.example
cp $infile $zonefile
ksk=`$KEYGEN -3 -q -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -3 -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  NSEC3/NSEC test zone
#
setup secure.nsec3.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  NSEC3/NSEC3 test zone
#
setup nsec3.nsec3.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC3 test zone
#
setup optout.nsec3.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A nsec3 zone (non-optout).
#
setup nsec3.example
cat $infile dsset-*.${zone}. > $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# An NSEC3 zone, with NSEC3 parameters set prior to signing
#
setup autonsec3.example
cat $infile > $zonefile
ksk=`$KEYGEN -G -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
echo $ksk > ../autoksk.key
zsk=`$KEYGEN -G -q -3 -r $RANDFILE $zone 2> kg.out` || dumpit kg.out
echo $zsk > ../autozsk.key
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC test zone
#
setup secure.optout.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC3 test zone
#
setup nsec3.optout.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/OPTOUT test zone
#
setup optout.optout.example
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A optout nsec3 zone.
#
setup optout.example
cat $infile dsset-*.${zone}. > $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A RSASHA256 zone.
#
setup rsasha256.example
cp $infile $zonefile
ksk=`$KEYGEN -q -a RSASHA256 -b 2048 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -a RSASHA256 -b 1024 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A RSASHA512 zone.
#
setup rsasha512.example
cp $infile $zonefile
ksk=`$KEYGEN -q -a RSASHA512 -b 2048 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -a RSASHA512 -b 1024 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# NSEC-only zone.
#
setup nsec.example
cp $infile $zonefile
ksk=`$KEYGEN -q -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
$KEYGEN -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# Signature refresh test zone.  Signatures are set to expire long
# in the past; they should be updated by autosign.
#
setup oldsigs.example
cp $infile $zonefile
$KEYGEN -q -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$SIGNER -PS -s now-1y -e now-6mo -o $zone -f $zonefile $infile > s.out 2>&1 || dumpit s.out

#
# NSEC3->NSEC transition test zone.
#
setup nsec3-to-nsec.example
$KEYGEN -q -a RSASHA512 -b 2048 -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -q -a RSASHA512 -b 1024 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$SIGNER -S -3 beef -A -o $zone -f $zonefile $infile > s.out 2>&1 || dumpit s.out

#
# secure-to-insecure transition test zone; used to test removal of
# keys via nsupdate
#
setup secure-to-insecure.example
$KEYGEN -q -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$SIGNER -S -o $zone -f $zonefile $infile > s.out 2>&1 || dumpit s.out

#
# another secure-to-insecure transition test zone; used to test
# removal of keys on schedule.
#
setup secure-to-insecure2.example
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
echo $ksk > ../del1.key
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone 2> kg.out` || dumpit kg.out
echo $zsk > ../del2.key
$SIGNER -S -3 beef -o $zone -f $zonefile $infile > s.out 2>&1 || dumpit s.out

#
# Introducing a pre-published key test.
#
setup prepub.example
infile="secure-to-insecure2.example.db.in"
$KEYGEN -3 -q -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -3 -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
$SIGNER -S -3 beef -o $zone -f $zonefile $infile > s.out 2>&1 || dumpit s.out

#
# Key TTL tests.
#

# no default key TTL; DNSKEY should get SOA TTL
setup ttl1.example
$KEYGEN -3 -q -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -3 -q -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
cp $infile $zonefile

# default key TTL should be used
setup ttl2.example 
$KEYGEN -3 -q -r $RANDFILE -fk -L 60 $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -3 -q -r $RANDFILE -L 60 $zone > kg.out 2>&1 || dumpit kg.out
cp $infile $zonefile

# mismatched key TTLs, should use shortest
setup ttl3.example
$KEYGEN -3 -q -r $RANDFILE -fk -L 30 $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -3 -q -r $RANDFILE -L 60 $zone > kg.out 2>&1 || dumpit kg.out
cp $infile $zonefile

# existing DNSKEY RRset, should retain TTL
setup ttl4.example
$KEYGEN -3 -q -r $RANDFILE -L 30 -fk $zone > kg.out 2>&1 || dumpit kg.out
cat ${infile} K${zone}.+*.key > $zonefile
$KEYGEN -3 -q -r $RANDFILE -L 180 $zone > kg.out 2>&1 || dumpit kg.out

#
# A zone with a DNSKEY RRset that is published before it's activated
#
setup delay.example
ksk=`$KEYGEN -G -q -3 -r $RANDFILE -fk $zone 2> kg.out` || dumpit kg.out
echo $ksk > ../delayksk.key
zsk=`$KEYGEN -G -q -3 -r $RANDFILE $zone 2> kg.out` || dumpit kg.out
echo $zsk > ../delayzsk.key

#
# A zone with signatures that are already expired, and the private ZSK
# is missing.
#
setup nozsk.example
$KEYGEN -q -3 -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone`
$SIGNER -S -P -s now-1mo -e now-1mi -o $zone -f $zonefile ${zonefile}.in > s.out 2>&1 || dumpit s.out
echo $zsk > ../missingzsk.key
rm -f ${zsk}.private

#
# A zone with signatures that are already expired, and the private ZSK
# is inactive.
#
setup inaczsk.example
$KEYGEN -q -3 -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone`
$SIGNER -S -P -s now-1mo -e now-1mi -o $zone -f $zonefile ${zonefile}.in > s.out 2>&1 || dumpit s.out
echo $zsk > ../inactivezsk.key
$SETTIME -I now $zsk > st.out 2>&1 || dumpit st.out

#
# A zone that is set to 'auto-dnssec maintain' during a recofnig
#
setup reconf.example
cp secure.example.db.in $zonefile
$KEYGEN -q -3 -r $RANDFILE -fk $zone > kg.out 2>&1 || dumpit kg.out
$KEYGEN -q -3 -r $RANDFILE $zone > kg.out 2>&1 || dumpit kg.out
