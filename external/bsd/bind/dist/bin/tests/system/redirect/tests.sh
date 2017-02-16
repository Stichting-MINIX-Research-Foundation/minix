#!/bin/sh
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

# Id: tests.sh,v 1.3 2011/03/01 23:48:06 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=1

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd -p 5300"

for conf in conf/good*.conf
do
        echo "I:checking that $conf is accepted ($n)"
        ret=0
        $CHECKCONF "$conf" || ret=1
        n=`expr $n + 1`
        if [ $ret != 0 ]; then echo "I:failed"; fi
        status=`expr $status + $ret`
done

for conf in conf/bad*.conf
do
        echo "I:checking that $conf is rejected ($n)"
        ret=0
        $CHECKCONF "$conf" >/dev/null && ret=1
	n=`expr $n + 1`
        if [ $ret != 0 ]; then echo "I:failed"; fi
        status=`expr $status + $ret`
done

echo "I:checking A redirect works for nonexist ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect works for nonexist ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.2 aaaa > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect works for nonexist ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.2 any > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect doesn't work for acl miss ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.4 a > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect doesn't work for acl miss ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.4 aaaa > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect doesn't work for acl miss ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.4 any > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect works for signed nonexist, DO=0 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.2 -b 10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect works for signed nonexist, DO=0 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.2 -b 10.53.0.2 aaaa > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect works for signed nonexist, DO=0 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.2 -b 10.53.0.2 any > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect fails for signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.2 -b 10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect fails for signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.2 -b 10.53.0.2 aaaa > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect fails for signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.2 -b 10.53.0.2 any > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect fails for nsec3 signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.2 -b 10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect fails for nsec3 signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.2 -b 10.53.0.2 aaaa > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect fails for nsec3 signed nonexist, DO=1 ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.2 -b 10.53.0.2 any > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "100.100.100.1" dig.out.ns2.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6401" dig.out.ns2.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect works for nonexist authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.1 a > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect works for nonexist authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.1 aaaa > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect works for nonexist authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.1 any > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect doesn't work for acl miss authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.4 a > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect doesn't work for acl miss authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.4 aaaa > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect doesn't work for acl miss authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.1 -b 10.53.0.4 any > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect works for signed nonexist, DO=0 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.1 -b 10.53.0.1 a > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect works for signed nonexist, DO=0 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.1 -b 10.53.0.1 aaaa > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect works for signed nonexist, DO=0 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. @10.53.0.1 -b 10.53.0.1 any > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect fails for signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.1 -b 10.53.0.1 a > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect fails for signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.1 -b 10.53.0.1 aaaa > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect fails for signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.signed. +dnssec @10.53.0.1 -b 10.53.0.1 any > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking A redirect fails for nsec3 signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.1 -b 10.53.0.1 a > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking AAAA redirect fails for nsec3 signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.1 -b 10.53.0.1 aaaa > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking ANY redirect fails for nsec3 signed nonexist, DO=1 authoritative ($n)"
ret=0
$DIG $DIGOPTS nonexist.nsec3. +dnssec @10.53.0.1 -b 10.53.0.1 any > dig.out.ns1.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns1.test$n > /dev/null || ret=1
grep "100.100.100.2" dig.out.ns1.test$n > /dev/null && ret=1
grep "2001:ffff:ffff::6464:6402" dig.out.ns1.test$n > /dev/null && ret=1
grep "IN.NSEC3" dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking redirect works (with noerror) when qtype is not found ($n)"
ret=0
$DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.2 txt > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that redirect zones reload correctly"
ret=0
sleep 1 # ensure file mtime will have changed
sed -e 's/0 0 0 0 0/1 0 0 0 0/' < ns2/example.db.in > ns2/example.db
sed -e 's/0 0 0 0 0/1 0 0 0 0/' -e 's/\.1$/.2/' < ns2/redirect.db.in > ns2/redirect.db
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload > rndc.out || ret=1
sed 's/^/I:ns2 /' rndc.out
for i in 1 2 3 4 5 6 7 8 9; do
    tmp=0
    $DIG $DIGOPTS +short @10.53.0.2 soa example.nil > dig.out.ns1.test$n || tmp=1
    set -- `cat dig.out.ns1.test$n`
    [ $3 = 1 ] || tmp=1
    $DIG $DIGOPTS nonexist. @10.53.0.2 -b 10.53.0.2 a > dig.out.ns2.test$n || tmp=1
    grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || tmp=1
    grep "100.100.100.2" dig.out.ns2.test$n > /dev/null || tmp=1
    [ $tmp -eq 0 ] && break
    sleep 1
done
[ $tmp -eq 1 ] && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
