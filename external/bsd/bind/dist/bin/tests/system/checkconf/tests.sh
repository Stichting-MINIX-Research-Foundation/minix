# Copyright (C) 2005, 2007, 2010-2015  Internet Systems Consortium, Inc. ("ISC")
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

# Id

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I: checking that named-checkconf handles a known good config"
ret=0
$CHECKCONF good.conf > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf prints a known good config"
ret=0
awk 'BEGIN { ok = 0; } /cut here/ { ok = 1; getline } ok == 1 { print }' good.conf > good.conf.in
[ -s good.conf.in ] || ret=1
$CHECKCONF -p good.conf.in | grep -v '^good.conf.in:' > good.conf.out 2>&1 || ret=1
cmp good.conf.in good.conf.out || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf -x removes secrets"
ret=0
# ensure there is a secret and that it is not the check string.
grep 'secret "' good.conf.in > /dev/null || ret=1
grep 'secret "????????????????"' good.conf.in > /dev/null 2>&1 && ret=1
$CHECKCONF -p -x good.conf.in | grep -v '^good.conf.in:' > good.conf.out 2>&1 || ret=1
grep 'secret "????????????????"' good.conf.out > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

for bad in bad*.conf
do
	ret=0
	echo "I: checking that named-checkconf detects error in $bad"
	$CHECKCONF $bad > /dev/null 2>&1
	if [ $? != 1 ]; then echo "I:failed"; ret=1; fi
	status=`expr $status + $ret`
done

echo "I: checking that named-checkconf -z catches missing hint file"
ret=0
$CHECKCONF -z hint-nofile.conf > hint-nofile.out 2>&1 && ret=1
grep "could not configure root hints from 'nonexistent.db': file not found" hint-nofile.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf catches range errors"
ret=0
$CHECKCONF range.conf > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf warns of notify inconsistencies"
ret=0
warnings=`$CHECKCONF notify.conf 2>&1 | grep "'notify' is disabled" | wc -l`
[ $warnings -eq 3 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking named-checkconf dnssec warnings"
ret=0
$CHECKCONF dnssec.1 2>&1 | grep 'validation yes.*enable no' > /dev/null || ret=1
$CHECKCONF dnssec.2 2>&1 | grep 'auto-dnssec may only be ' > /dev/null || ret=1
$CHECKCONF dnssec.2 2>&1 | grep 'validation auto.*enable no' > /dev/null || ret=1
$CHECKCONF dnssec.2 2>&1 | grep 'validation yes.*enable no' > /dev/null || ret=1
# this one should have no warnings
$CHECKCONF dnssec.3 2>&1 | grep '.*' && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: range checking fields that do not allow zero"
ret=0
for field in max-retry-time min-retry-time max-refresh-time min-refresh-time; do
    cat > badzero.conf << EOF
options {
    $field 0;
};
EOF
    $CHECKCONF badzero.conf > /dev/null 2>&1
    [ $? -eq 1 ] || { echo "I: options $field failed" ; ret=1; }
    cat > badzero.conf << EOF
view dummy {
    $field 0;
};
EOF
    $CHECKCONF badzero.conf > /dev/null 2>&1
    [ $? -eq 1 ] || { echo "I: view $field failed" ; ret=1; }
    cat > badzero.conf << EOF
options {
    $field 0;
};
view dummy {
};
EOF
    $CHECKCONF badzero.conf > /dev/null 2>&1
    [ $? -eq 1 ] || { echo "I: options + view $field failed" ; ret=1; }
    cat > badzero.conf << EOF
zone dummy {
    type slave;
    masters { 0.0.0.0; };
    $field 0;
};
EOF
    $CHECKCONF badzero.conf > /dev/null 2>&1
    [ $? -eq 1 ] || { echo "I: zone $field failed" ; ret=1; }
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking options allowed in inline-signing slaves"
ret=0
n=`$CHECKCONF bad-dnssec.conf 2>&1 | grep "dnssec-dnskey-kskonly.*requires inline" | wc -l`
[ $n -eq 1 ] || ret=1
n=`$CHECKCONF bad-dnssec.conf 2>&1 | grep "dnssec-loadkeys-interval.*requires inline" | wc -l`
[ $n -eq 1 ] || ret=1
n=`$CHECKCONF bad-dnssec.conf 2>&1 | grep "update-check-ksk.*requires inline" | wc -l`
[ $n -eq 1 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: check file + inline-signing for slave zones"
n=`$CHECKCONF inline-no.conf 2>&1 | grep "missing 'file' entry" | wc -l`
[ $n -eq 0 ] || ret=1
n=`$CHECKCONF inline-good.conf 2>&1 | grep "missing 'file' entry" | wc -l`
[ $n -eq 0 ] || ret=1
n=`$CHECKCONF inline-bad.conf 2>&1 | grep "missing 'file' entry" | wc -l`
[ $n -eq 1 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking named-checkconf DLZ warnings"
ret=0
$CHECKCONF dlz-bad.conf 2>&1 | grep "'dlz' and 'database'" > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking for missing key directory warning"
ret=0
rm -rf test.keydir
n=`$CHECKCONF warn-keydir.conf 2>&1 | grep "'test.keydir' does not exist" | wc -l`
[ $n -eq 1 ] || ret=1
touch test.keydir
n=`$CHECKCONF warn-keydir.conf 2>&1 | grep "'test.keydir' is not a directory" | wc -l`
[ $n -eq 1 ] || ret=1
rm -f test.keydir
mkdir test.keydir
n=`$CHECKCONF warn-keydir.conf 2>&1 | grep "key-directory" | wc -l`
[ $n -eq 0 ] || ret=1
rm -rf test.keydir
if [ $ret != 0 ]; then echo "I:failed"; fi

echo "I: checking that named-checkconf -z catches conflicting ttl with max-ttl"
ret=0
$CHECKCONF -z max-ttl.conf > check.out 2>&1
grep 'TTL 900 exceeds configured max-zone-ttl 600' check.out > /dev/null 2>&1 || ret=1
grep 'TTL 900 exceeds configured max-zone-ttl 600' check.out > /dev/null 2>&1 || ret=1
grep 'TTL 900 exceeds configured max-zone-ttl 600' check.out > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf -z catches invalid max-ttl"
ret=0
$CHECKCONF -z max-ttl-bad.conf > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf -z skips zone check with alternate databases"
ret=0
$CHECKCONF -z altdb.conf > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf -z skips zone check with DLZ"
ret=0
$CHECKCONF -z altdlz.conf > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-names fails as configured"
ret=0
$CHECKCONF -z check-names-fail.conf > checkconf.out1 2>&1 && ret=1
grep "near '_underscore': bad name (check-names)" checkconf.out1 > /dev/null || ret=1
grep "zone check-names/IN: loaded serial" < checkconf.out1 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-mx fails as configured"
ret=0
$CHECKCONF -z check-mx-fail.conf > checkconf.out2 2>&1 && ret=1
grep "near '10.0.0.1': MX is an address" checkconf.out2 > /dev/null || ret=1
grep "zone check-mx/IN: loaded serial" < checkconf.out2 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-dup-records fails as configured"
ret=0
$CHECKCONF -z check-dup-records-fail.conf > checkconf.out3 2>&1 && ret=1
grep "has semantically identical records" checkconf.out3 > /dev/null || ret=1
grep "zone check-dup-records/IN: loaded serial" < checkconf.out3 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-mx fails as configured"
ret=0
$CHECKCONF -z check-mx-fail.conf > checkconf.out4 2>&1 && ret=1
grep "failed: MX is an address" checkconf.out4 > /dev/null || ret=1
grep "zone check-mx/IN: loaded serial" < checkconf.out4 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-mx-cname fails as configured"
ret=0
$CHECKCONF -z check-mx-cname-fail.conf > checkconf.out5 2>&1 && ret=1
grep "MX.* is a CNAME (illegal)" checkconf.out5 > /dev/null || ret=1
grep "zone check-mx-cname/IN: loaded serial" < checkconf.out5 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I: check that check-srv-cname fails as configured"
ret=0
$CHECKCONF -z check-srv-cname-fail.conf > checkconf.out6 2>&1 && ret=1
grep "SRV.* is a CNAME (illegal)" checkconf.out6 > /dev/null || ret=1
grep "zone check-mx-cname/IN: loaded serial" < checkconf.out6 > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
