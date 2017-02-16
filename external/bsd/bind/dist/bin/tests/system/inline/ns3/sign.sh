#!/bin/sh -e
#
# Copyright (C) 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

zone=bits
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=noixfr
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=master
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=dynamic
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=updated
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db
$SIGNER -S -O raw -L 2000042407 -o ${zone} ${zone}.db > /dev/null 2>&1
cp master2.db.in updated.db

# signatures are expired and should be regenerated on startup
zone=expired
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db
$SIGNER -PS -s 20100101000000 -e 20110101000000 -O raw -L 2000042407 -o ${zone} ${zone}.db > /dev/null 2>&1

zone=retransfer
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=nsec3
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

zone=retransfer3
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private
keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`
keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone -f KSK $zone`
$DSFROMKEY -T 1200 $keyname >> ../ns1/root.db

for s in a c d h k l m q z
do
	zone=test-$s
	keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
done

for s in b f i o p t v
do
	zone=test-$s
	keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
	keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -f KSK $zone`
done

zone=externalkey
rm -f K${zone}.+*+*.key
rm -f K${zone}.+*+*.private

for alg in ECCGOST ECDSAP256SHA256 NSEC3RSASHA1 DSA
do
    case $alg in
        DSA)
            $SHELL ../checkdsa.sh 2> /dev/null || continue
            checkfile=../checkdsa
            touch $checkfile ;;
        ECCGOST) 
            fail=0
            $KEYGEN -q -r $RANDFILE -a eccgost test > /dev/null 2>&1 || fail=1
            rm -f Ktest*
            [ $fail != 0 ] && continue
            checkfile=../checkgost
            touch $checkfile ;;
        ECDSAP256SHA256)
            fail=0
            $KEYGEN -q -r $RANDFILE -a ecdsap256sha256 test > /dev/null 2>&1 || fail=1
            rm -f Ktest*
            [ $fail != 0 ] && continue
            $SHELL ../checkdsa.sh 2> /dev/null || continue
            checkfile=../checkecdsa
            touch $checkfile ;;
        *) ;;
    esac

    k1=`$KEYGEN -q -r $RANDFILE -a $alg -b 1024 -n zone -f KSK $zone`
    k2=`$KEYGEN -q -r $RANDFILE -a $alg -b 1024 -n zone $zone`
    k3=`$KEYGEN -q -r $RANDFILE -a $alg -b 1024 -n zone $zone`
    k4=`$KEYGEN -q -r $RANDFILE -a $alg -b 1024 -n zone -f KSK $zone`
    $DSFROMKEY -T 1200 $k4 >> ../ns1/root.db

    # Convert k1 and k2 in to External Keys.
    rm -f $k1.private 
    mv $k1.key a-file
    $IMPORTKEY -P now -D now+3600 -f a-file $zone > /dev/null 2>&1 ||
        ( echo "importkey failed: $alg"; rm -f $checkfile )
    rm -f $k2.private 
    mv $k2.key a-file
    $IMPORTKEY -f a-file $zone > /dev/null 2>&1 ||
        ( echo "importkey failed: $alg"; rm -f $checkfile )
done
