#!/bin/sh
#
# Copyright (C) 2010-2014  Internet Systems Consortium, Inc. ("ISC")
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

# tests for TSIG-GSS updates

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

DIGOPTS="@10.53.0.1 -p 5300"

test_update() {
    host="$1"
    type="$2"
    cmd="$3"
    digout="$4"
    should_fail="$5"

    cat <<EOF > ns1/update.txt
server 10.53.0.1 5300
update add $host $cmd
send
EOF

    echo "I:testing update for $host $type $cmd $comment"
    $NSUPDATE -k ns1/ddns.key ns1/update.txt > /dev/null 2>&1 || {
	[ "$should_fail" ] || \
             echo "I:update failed for $host $type $cmd"
	return 1
    }

    out=`$DIG $DIGOPTS -t $type -q $host | egrep "^$host"`
    lines=`echo "$out" | grep "$digout" | wc -l`
    [ $lines -eq 1 ] || {
	[ "$should_fail" ] || \
            echo "I:dig output incorrect for $host $type $cmd: $out"
	return 1
    }
    return 0
}

ret=0

test_update testdc1.example.nil. A "86400 A 10.53.0.10" "10.53.0.10" || ret=1
status=`expr $status + $ret`

test_update testdc2.example.nil. A "86400 A 10.53.0.11" "10.53.0.11" || ret=1
status=`expr $status + $ret`

test_update testdc3.example.nil. A "86400 A 10.53.0.10" "10.53.0.10" || ret=1
status=`expr $status + $ret`

test_update deny.example.nil. TXT "86400 TXT helloworld" "helloworld" should_fail && ret=1
status=`expr $status + $ret`

echo "I:testing passing client info into DLZ driver"
ret=0
out=`$DIG $DIGOPTS +short -t txt -q source-addr.example.nil | grep -v '^;'`
addr=`eval echo "$out" | cut -f1 -d'#'`
[ "$addr" = "10.53.0.1" ] || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing DLZ driver is cleaned up on reload"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload 2>&1 | sed 's/^/I:ns1 /'
for i in 0 1 2 3 4 5 6 7 8 9; do
    ret=0
    grep 'dlz_example: shutting down zone example.nil' ns1/named.run > /dev/null 2>&1 || ret=1
    [ "$ret" -eq 0 ] && break
    sleep 1
done
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing multiple DLZ drivers"
test_update testdc1.alternate.nil. A "86400 A 10.53.0.10" "10.53.0.10" || ret=1
status=`expr $status + $ret`

ret=0
echo "I:testing AXFR from DLZ drivers"
$DIG $DIGOPTS +noall +answer axfr example.nil > dig.out.ns1.1
n=`cat dig.out.ns1.1 | wc -l`
[ "$n" -eq 4 ] || ret=1
$DIG $DIGOPTS +noall +answer axfr alternate.nil > dig.out.ns1.2
n=`cat dig.out.ns1.2 | wc -l`
[ "$n" -eq 5 ] || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing unsearched/unregistered DLZ zone is not found"
$DIG $DIGOPTS +noall +answer ns other.nil > dig.out.ns1.3
grep "3600.IN.NS.other.nil." dig.out.ns1.3 > /dev/null && ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing unsearched/registered DLZ zone is found"
$DIG $DIGOPTS +noall +answer ns zone.nil > dig.out.ns1.4
grep "3600.IN.NS.zone.nil." dig.out.ns1.4 > /dev/null || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing unsearched/registered DLZ zone is found"
$DIG $DIGOPTS +noall +answer ns zone.nil > dig.out.ns1.5
grep "3600.IN.NS.zone.nil." dig.out.ns1.5 > /dev/null || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing correct behavior with findzone returning ISC_R_NOMORE"
$DIG $DIGOPTS +noall a test.example.com > /dev/null 2>&1 || ret=1
# we should only find one logged lookup per searched DLZ database
lines=`grep "dlz_findzonedb.*test\.example\.com.*example.nil" ns1/named.run | wc -l`
[ $lines -eq 1 ] || ret=1
lines=`grep "dlz_findzonedb.*test\.example\.com.*alternate.nil" ns1/named.run | wc -l`
[ $lines -eq 1 ] || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing findzone can return different results per client"
$DIG $DIGOPTS -b 10.53.0.1 +noall a test.example.net > /dev/null 2>&1 || ret=1
# we should only find one logged lookup per searched DLZ database
lines=`grep "dlz_findzonedb.*example\.net.*example.nil" ns1/named.run | wc -l`
[ $lines -eq 1 ] || ret=1
lines=`grep "dlz_findzonedb.*example\.net.*alternate.nil" ns1/named.run | wc -l`
[ $lines -eq 1 ] || ret=1
$DIG $DIGOPTS -b 10.53.0.2 +noall a test.example.net > /dev/null 2>&1 || ret=1
# we should find several logged lookups this time
lines=`grep "dlz_findzonedb.*example\.net.*example.nil" ns1/named.run | wc -l`
[ $lines -gt 2 ] || ret=1
lines=`grep "dlz_findzonedb.*example\.net.*alternate.nil" ns1/named.run | wc -l`
[ $lines -gt 2 ] || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing zone returning oversized data"
$DIG $DIGOPTS txt too-long.example.nil > dig.out.ns1.6 2>&1 || ret=1
grep "status: SERVFAIL" dig.out.ns1.6 > /dev/null || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

ret=0
echo "I:testing zone returning oversized data at zone origin"
$DIG $DIGOPTS txt bigcname.domain > dig.out.ns1.7 2>&1 || ret=1
grep "status: SERVFAIL" dig.out.ns1.7 > /dev/null || ret=1
[ "$ret" -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

exit $status
