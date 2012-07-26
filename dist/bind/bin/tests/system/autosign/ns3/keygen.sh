#!/bin/sh -e
#
# Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: keygen.sh,v 1.8.18.3 2011-07-08 01:45:58 each Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=secure.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -3 -q -r $RANDFILE -fk $zone`
$KEYGEN -3 -q -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  NSEC3/NSEC test zone
#
zone=secure.nsec3.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  NSEC3/NSEC3 test zone
#
zone=nsec3.nsec3.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC3 test zone
#
zone=optout.nsec3.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A nsec3 zone (non-optout).
#
zone=nsec3.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cat $infile dsset-*.${zone}. > $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC test zone
#
zone=secure.optout.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/NSEC3 test zone
#
zone=nsec3.optout.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
#  OPTOUT/OPTOUT test zone
#
zone=optout.optout.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A optout nsec3 zone.
#
zone=optout.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cat $infile dsset-*.${zone}. > $zonefile
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
$KEYGEN -q -3 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A RSASHA256 zone.
#
zone=rsasha256.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -a RSASHA256 -b 2048 -r $RANDFILE -fk $zone`
$KEYGEN -q -a RSASHA256 -b 1024 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# A RSASHA512 zone.
#
zone=rsasha512.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -a RSASHA512 -b 2048 -r $RANDFILE -fk $zone`
$KEYGEN -q -a RSASHA512 -b 1024 -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# NSEC-only zone.
#
zone=nsec.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -r $RANDFILE -fk $zone`
$KEYGEN -q -r $RANDFILE $zone > /dev/null
$DSFROMKEY $ksk.key > dsset-${zone}.

#
# Signature refresh test zone.  Signatures are set to expire long
# in the past; they should be updated by autosign.
#
zone=oldsigs.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -r $RANDFILE -fk $zone`
$KEYGEN -q -r $RANDFILE $zone > /dev/null
$SIGNER -PS -s now-1y -e now-6mo -o $zone -f $zonefile $infile > /dev/null 2>&1

#
# NSEC3->NSEC transition test zone.
#
zone=nsec3-to-nsec.example
zonefile="${zone}.db"
infile="${zonefile}.in"
cp $infile $zonefile
ksk=`$KEYGEN -q -a RSASHA512 -b 2048 -r $RANDFILE -fk $zone`
$KEYGEN -q -a RSASHA512 -b 1024 -r $RANDFILE $zone > /dev/null
$SIGNER -S -3 beef -A -o $zone -f $zonefile $infile > /dev/null 2>&1

#
# secure-to-insecure transition test zone; used to test removal of
# keys via nsupdate
#
zone=secure-to-insecure.example
zonefile="${zone}.db"
infile="${zonefile}.in"
ksk=`$KEYGEN -q -r $RANDFILE -fk $zone`
$KEYGEN -q -r $RANDFILE $zone > /dev/null
$SIGNER -S -o $zone -f $zonefile $infile > /dev/null 2>&1

#
# another secure-to-insecure transition test zone; used to test
# removal of keys on schedule.
#
zone=secure-to-insecure2.example
zonefile="${zone}.db"
infile="${zonefile}.in"
ksk=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
echo $ksk > ../del1.key
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone`
echo $zsk > ../del2.key
$SIGNER -S -3 beef -o $zone -f $zonefile $infile > /dev/null 2>&1

#
# Introducing a pre-published key test.
#
zone=prepub.example
zonefile="${zone}.db"
$KEYGEN -3 -q -r $RANDFILE -fk $zone > /dev/null
$KEYGEN -3 -q -r $RANDFILE $zone > /dev/null
$SIGNER -S -3 beef -o $zone -f $zonefile $infile > /dev/null 2>&1

#
# A zone with a DNSKEY RRset that is published before it's activated
#
zone=delay.example
zonefile="${zone}.db"
ksk=`$KEYGEN -G -q -3 -r $RANDFILE -fk $zone`
echo $ksk > ../delayksk.key
zsk=`$KEYGEN -G -q -3 -r $RANDFILE $zone`
echo $zsk > ../delayzsk.key

#
# A zone with signatures that are already expired, and the private ZSK
# is missing.
#
zone=nozsk.example
zonefile="${zone}.db"
$KEYGEN -q -3 -r $RANDFILE -fk $zone > /dev/null
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone`
$SIGNER -S -P -s now-1mo -e now-1mi -o $zone -f $zonefile ${zonefile}.in > /dev/null 2>&1
echo $zsk > ../missingzsk.key
rm -f ${zsk}.private

#
# A zone with signatures that are already expired, and the private ZSK
# is inactive.
#
zone=inaczsk.example
zonefile="${zone}.db"
$KEYGEN -q -3 -r $RANDFILE -fk $zone > /dev/null
zsk=`$KEYGEN -q -3 -r $RANDFILE $zone`
$SIGNER -S -P -s now-1mo -e now-1mi -o $zone -f $zonefile ${zonefile}.in > /dev/null 2>&1
echo $zsk > ../inactivezsk.key
$SETTIME -I now $zsk > /dev/null
