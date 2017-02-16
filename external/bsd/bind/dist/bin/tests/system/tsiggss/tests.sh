#!/bin/sh
#
# Copyright (C) 2010, 2011, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# we don't want a KRB5_CONFIG setting breaking the tests
KRB5_CONFIG=/dev/null
export KRB5_CONFIG

test_update() {
    host="$1"
    type="$2"
    cmd="$3"
    digout="$4"

    cat <<EOF > ns1/update.txt
server 10.53.0.1 5300
update add $host $cmd
send
EOF
    echo "I:testing update for $host $type $cmd"
    $NSUPDATE -g -d ns1/update.txt > nsupdate.out 2>&1 || {
	echo "I:update failed for $host $type $cmd"
	sed "s/^/I:/" nsupdate.out
	return 1
    }

    out=`$DIG $DIGOPTS -t $type -q $host | egrep "^${host}"`
    lines=`echo "$out" | grep "$digout" | wc -l`
    [ $lines -eq 1 ] || {
	echo "I:dig output incorrect for $host $type $cmd: $out"
	return 1
    }
    return 0
}

echo "I:testing updates as administrator"
KRB5CCNAME="FILE:"`pwd`/ns1/administrator.ccache
export KRB5CCNAME

test_update testdc1.example.nil. A "86400 A 10.53.0.10" "10.53.0.10" || status=1
test_update testdc2.example.nil. A "86400 A 10.53.0.11" "10.53.0.11" || status=1
test_update denied.example.nil. TXT "86400 TXT helloworld" "helloworld" > /dev/null && status=1

echo "I:testing updates as a user"
KRB5CCNAME="FILE:"`pwd`/ns1/testdenied.ccache
export KRB5CCNAME

test_update testdenied.example.nil. A "86400 A 10.53.0.12" "10.53.0.12" > /dev/null && status=1
test_update testdenied.example.nil. TXT "86400 TXT helloworld" "helloworld" || status=1

echo "I:testing external update policy"
test_update testcname.example.nil. TXT "86400 CNAME testdenied.example.nil" "testdenied" > /dev/null && status=1
$PERL ./authsock.pl --type=CNAME --path=ns1/auth.sock --pidfile=authsock.pid --timeout=120 > /dev/null 2>&1 &
sleep 1
test_update testcname.example.nil. TXT "86400 CNAME testdenied.example.nil" "testdenied" || status=1
test_update testcname.example.nil. TXT "86400 A 10.53.0.13" "10.53.0.13" > /dev/null && status=1

echo "I:testing external policy with SIG(0) key"
ret=0
$NSUPDATE -R $RANDFILE -k ns1/Kkey.example.nil.*.private <<END > /dev/null 2>&1 || ret=1
server 10.53.0.1 5300
zone example.nil
update add fred.example.nil 120 cname foo.bar.
send
END
output=`$DIG $DIGOPTS +short cname fred.example.nil.`
[ -n "$output" ] || ret=1
[ $ret -eq 0 ] || echo "I:failed"
status=`expr $status + $ret`

[ $status -eq 0 ] && echo "I:tsiggss tests all OK"

kill `cat authsock.pid`
exit $status
