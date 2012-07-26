#!/bin/sh
#
# Copyright (C) 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.6.70.3 2011-07-08 01:45:58 each Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

RANDFILE=./random.data

pzone=parent.nil
pfile=parent.db

czone=child.parent.nil
cfile=child.db

echo I:generating keys
# active zsk
czsk1=`$KEYGEN -q -r $RANDFILE $czone`

# not yet published or active
czsk2=`$KEYGEN -q -r $RANDFILE -P none -A none $czone`

# published but not active
czsk3=`$KEYGEN -q -r $RANDFILE -A none $czone`

# inactive
czsk4=`$KEYGEN -q -r $RANDFILE -P now-24h -A now-24h -I now $czone`

# active in 12 hours, inactive 12 hours after that...
czsk5=`$KEYGEN -q -r $RANDFILE -P now+12h -A now+12h -I now+24h $czone`

# explicit successor to czk5
# (suppressing warning about lack of removal date)
czsk6=`$KEYGEN -q -r $RANDFILE -S $czsk5 -i 6h 2>&-` 

# active ksk
cksk1=`$KEYGEN -q -r $RANDFILE -fk $czone`

# published but not YET active; will be active in 20 seconds
cksk2=`$KEYGEN -q -r $RANDFILE -fk $czone`
# $SETTIME moved after other $KEYGENs

echo I:revoking key
# revoking key changes its ID
cksk3=`$KEYGEN -q -r $RANDFILE -fk $czone`
cksk4=`$REVOKE $cksk3`
$SETTIME -A now+20s $cksk2 > /dev/null

echo I:signing child zone
czoneout=`$SIGNER -Sg -r $RANDFILE -o $czone $cfile 2>&1`

echo I:generating keys
pzsk=`$KEYGEN -q -r $RANDFILE $pzone`
pksk=`$KEYGEN -q -r $RANDFILE -fk $pzone`

echo I:signing parent zone
pzoneout=`$SIGNER -Sg -r $RANDFILE -o $pzone $pfile 2>&1`

czactive=`echo $czsk1 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
czgenerated=`echo $czsk2 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
czpublished=`echo $czsk3 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
czinactive=`echo $czsk4 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
czpredecessor=`echo $czsk5 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
czsuccessor=`echo $czsk6 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
ckactive=`echo $cksk1 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
ckpublished=`echo $cksk2 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
ckprerevoke=`echo $cksk3 | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
ckrevoked=`echo $cksk4 | sed 's/.*+005+0*\([0-9]*\)$/\1/'`

pzid=`echo $pzsk | sed 's/^K.*+005+0*\([0-9]\)/\1/'`
pkid=`echo $pksk | sed 's/^K.*+005+0*\([0-9]\)/\1/'`

echo "I:checking dnssec-signzone output matches expectations"
ret=0
echo "$pzoneout" | grep 'KSKs: 1 active, 0 stand-by, 0 revoked' > /dev/null || ret=1
echo "$pzoneout" | grep 'ZSKs: 1 active, 0 stand-by, 0 revoked' > /dev/null || ret=1
echo "$czoneout" | grep 'KSKs: 1 active, 1 stand-by, 1 revoked' > /dev/null || ret=1
echo "$czoneout" | grep 'ZSKs: 1 active, 2 stand-by, 0 revoked' > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:rechecking dnssec-signzone output with -x"
ret=0
# use an alternate output file so -x doesn't interfere with later checks
pzoneout=`$SIGNER -Sxg -r $RANDFILE -o $pzone -f ${pzone}2.signed $pfile 2>&1`
czoneout=`$SIGNER -Sxg -r $RANDFILE -o $czone -f ${czone}2.signed $cfile 2>&1`
echo "$pzoneout" | grep 'KSKs: 1 active, 0 stand-by, 0 revoked' > /dev/null || ret=1
echo "$pzoneout"| grep 'ZSKs: 1 active, 0 present, 0 revoked' > /dev/null || ret=1
echo "$czoneout"| grep 'KSKs: 1 active, 1 stand-by, 1 revoked' > /dev/null || ret=1
echo "$czoneout"| grep 'ZSKs: 1 active, 2 present, 0 revoked' > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking parent zone DNSKEY set"
ret=0
grep "key id = $pzid" $pfile.signed > /dev/null || ret=1
grep "key id = $pkid" $pfile.signed > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking parent zone DS records"
ret=0
awk '$2 == "DS" {print $3}' $pfile.signed > dsset.out
grep "$ckactive" dsset.out > /dev/null || ret=1
grep "$ckpublished" dsset.out > /dev/null || ret=1
# revoked key should not be there, hence the &&
grep "$ckprerevoke" dsset.out > /dev/null && ret=1
grep "$ckrevoked" dsset.out > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking child zone DNSKEY set"
ret=0
grep "key id = $ckactive" $cfile.signed > /dev/null || ret=1
grep "key id = $ckpublished" $cfile.signed > /dev/null || ret=1
grep "key id = $ckrevoked" $cfile.signed > /dev/null || ret=1
grep "key id = $czactive" $cfile.signed > /dev/null || ret=1
grep "key id = $czpublished" $cfile.signed > /dev/null || ret=1
grep "key id = $czinactive" $cfile.signed > /dev/null || ret=1
# should not be there, hence the &&
grep "key id = $ckprerevoke" $cfile.signed > /dev/null && ret=1
grep "key id = $czgenerated" $cfile.signed > /dev/null && ret=1
grep "key id = $czpredecessor" $cfile.signed && echo pred is there
grep "key id = $czsuccessor" $cfile.signed && echo succ is there
#grep "key id = $czpredecessor" $cfile.signed > /dev/null && ret=1
#grep "key id = $czsuccessor" $cfile.signed > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking child zone signatures"
ret=0
# check DNSKEY signatures first
awk '$2 == "RRSIG" && $3 == "DNSKEY" { getline; print $2 }' $cfile.signed > dnskey.sigs
grep "$ckactive" dnskey.sigs > /dev/null || ret=1
grep "$ckrevoked" dnskey.sigs > /dev/null || ret=1
grep "$czactive" dnskey.sigs > /dev/null || ret=1
# should not be there:
grep "$ckprerevoke" dnskey.sigs > /dev/null && ret=1
grep "$ckpublished" dnskey.sigs > /dev/null && ret=1
grep "$czpublished" dnskey.sigs > /dev/null && ret=1
grep "$czinactive" dnskey.sigs > /dev/null && ret=1
grep "$czgenerated" dnskey.sigs > /dev/null && ret=1
# now check other signatures
awk '$2 == "RRSIG" && $3 != "DNSKEY" { getline; print $2 }' $cfile.signed | sort -un > other.sigs
# should not be there:
grep "$ckactive" other.sigs > /dev/null && ret=1
grep "$ckpublished" other.sigs > /dev/null && ret=1
grep "$ckprerevoke" other.sigs > /dev/null && ret=1
grep "$ckrevoked" other.sigs > /dev/null && ret=1
grep "$czpublished" other.sigs > /dev/null && ret=1
grep "$czinactive" other.sigs > /dev/null && ret=1
grep "$czgenerated" other.sigs > /dev/null && ret=1
grep "$czpredecessor" other.sigs > /dev/null && ret=1
grep "$czsuccessor" other.sigs > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:waiting 20 seconds for key activation"
sleep 20
echo "I:re-signing child zone"
czoneout2=`$SIGNER -Sg -r $RANDFILE -o $czone -f $cfile.new $cfile.signed 2>&1`
mv $cfile.new $cfile.signed

echo "I:checking dnssec-signzone output matches expectations"
ret=0
echo "$czoneout2" | grep 'KSKs: 2 active, 0 stand-by, 1 revoked' > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking child zone signatures again"
ret=0
awk '$2 == "RRSIG" && $3 == "DNSKEY" { getline; print $2 }' $cfile.signed > dnskey.sigs
grep "$ckpublished" dnskey.sigs > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
