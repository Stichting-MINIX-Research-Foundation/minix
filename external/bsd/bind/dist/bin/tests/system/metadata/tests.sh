#!/bin/sh
#
# Copyright (C) 2009, 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.9 2011/07/08 01:43:26 each Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

pzone=parent.nil pfile=parent.db
czone=child.parent.nil cfile=child.db
status=0
n=0

echo "I:setting key timers"
$SETTIME -A now+15s `cat rolling.key` > /dev/null

inact=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < inact.key`
ksk=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < ksk.key`
pending=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < pending.key`
postrev=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < postrev.key`
prerev=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < prerev.key`
rolling=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < rolling.key`
standby=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < standby.key`
zsk=`sed 's/^K'${czone}'.+005+0*\([0-9]\)/\1/' < zsk.key`

../../../tools/genrandom 400 $RANDFILE

echo "I:signing zones"
$SIGNER -Sg -o $czone $cfile > /dev/null 2>&1
$SIGNER -Sg -o $pzone $pfile > /dev/null 2>&1

awk '$2 ~ /RRSIG/ {
        type = $3;
        getline;
	id = $3;
	if ($4 ~ /'${czone}'/) {
		print type, id
	}
}' < ${cfile}.signed > sigs

awk '$2 ~ /DNSKEY/ {
	flags = $3;
	while ($0 !~ /key id =/)
		getline;
	id = $NF;
	print flags, id;
}' < ${cfile}.signed > keys

echo "I:checking that KSK signed DNSKEY only ($n)"
ret=0
grep "DNSKEY $ksk"'$' sigs > /dev/null || ret=1
grep "SOA $ksk"'$' sigs > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that ZSK signed ($n)"
ret=0
grep "SOA $zsk"'$' sigs > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that standby ZSK did not sign ($n)"
ret=0
grep " $standby"'$' sigs > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that inactive key did not sign ($n)"
ret=0
grep " $inact"'$' sigs > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that pending key was not published ($n)"
ret=0
grep " $pending"'$' keys > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that standby KSK did not sign but is delegated ($n)"
ret=0
grep " $rolling"'$' sigs > /dev/null && ret=1
grep " $rolling"'$' keys > /dev/null || ret=1
egrep "DS[ 	]*$rolling[ 	]" ${pfile}.signed > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that key was revoked ($n)"
ret=0
grep " $prerev"'$' keys > /dev/null && ret=1
grep " $postrev"'$' keys > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that revoked key self-signed ($n)"
ret=0
grep "DNSKEY $postrev"'$' sigs > /dev/null || ret=1
grep "SOA $postrev"'$' sigs > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:waiting 20 seconds for key changes to occur"
sleep 20

echo "I:re-signing zone"
$SIGNER  -Sg -o $czone -f ${cfile}.new ${cfile}.signed > /dev/null 2>&1

echo "I:checking that standby KSK is now active ($n)"
ret=0
grep "DNSKEY $rolling"'$' sigs > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking update of an old-style key ($n)"
ret=0
# printing metadata should not work with an old-style key
$SETTIME -pall `cat oldstyle.key` > /dev/null 2>&1 && ret=1
$SETTIME -f `cat oldstyle.key` > /dev/null 2>&1 || ret=1
# but now it should
$SETTIME -pall `cat oldstyle.key` > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking warning about permissions change on key with dnssec-settime ($n)"
ret=0
# settime should print a warning about changing the permissions
chmod 644 `cat oldstyle.key`.private
$SETTIME -P none `cat oldstyle.key` > tmp.out 2>&1 || ret=1
grep "warning" tmp.out > /dev/null 2>&1 || ret=1
$SETTIME -P none `cat oldstyle.key` > tmp.out 2>&1 || ret=1
grep "warning" tmp.out > /dev/null 2>&1 && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking warning about delete date < inactive date with dnssec-settime ($n)"
ret=0
# settime should print a warning about delete < inactive
$SETTIME -I now+15s -D now `cat oldstyle.key` > tmp.out 2>&1 || ret=1
grep "warning" tmp.out > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking warning about delete date < inactive date with dnssec-keygen ($n)"
ret=0
# keygen should print a warning about delete < inactive
$KEYGEN -q -r $RANDFILE -I now+15s -D now $czone > tmp.out 2>&1 || ret=1
grep "warning" tmp.out > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking correct behavior setting activation without publication date ($n)"
ret=0
key=`$KEYGEN -q -r $RANDFILE -A +1w $czone`
pub=`$SETTIME -upP $key | awk '{print $2}'`
act=`$SETTIME -upA $key | awk '{print $2}'`
[ $pub -eq $act ] || ret=1
key=`$KEYGEN -q -r $RANDFILE -A +1w -i 1d $czone`
pub=`$SETTIME -upP $key | awk '{print $2}'`
act=`$SETTIME -upA $key | awk '{print $2}'`
[ $pub -lt $act ] || ret=1
key=`$KEYGEN -q -r $RANDFILE -A +1w -P never $czone`
pub=`$SETTIME -upP $key | awk '{print $2}'`
[ $pub = "UNSET" ] || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
