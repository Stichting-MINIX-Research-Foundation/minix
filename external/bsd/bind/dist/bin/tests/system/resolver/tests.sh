#!/bin/sh
#
# Copyright (C) 2004, 2007, 2009-2014  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

# Id: tests.sh,v 1.22 2012/02/09 23:47:18 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

echo "I:checking non-cachable NXDOMAIN response handling"
ret=0
$DIG +tcp nxdomain.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NXDOMAIN" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
echo "I:checking non-cachable NXDOMAIN response handling using dns_client"
   ret=0
   ${RESOLVE} -p 5300 -t a -s 10.53.0.1 nxdomain.example.net 2> resolve.out || ret=1
   grep "resolution failed: ncache nxdomain" resolve.out > /dev/null || ret=1
   if [ $ret != 0 ]; then echo "I:failed"; fi
   status=`expr $status + $ret`
fi

if [ -x ${RESOLVE} ] ; then
echo "I:checking that local bound address can be set (Can't query from a denied address)"
   ret=0
   ${RESOLVE} -b 10.53.0.8 -p 5300 -t a -s 10.53.0.1 www.example.org 2> resolve.out || ret=1
   grep "resolution failed: failure" resolve.out > /dev/null || ret=1
   if [ $ret != 0 ]; then echo "I:failed"; fi
   status=`expr $status + $ret`

echo "I:checking that local bound address can be set (Can query from an allowed address)"
   ret=0
   ${RESOLVE} -b 10.53.0.1 -p 5300 -t a -s 10.53.0.1 www.example.org > resolve.out || ret=1
   grep "www.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
   if [ $ret != 0 ]; then echo "I:failed"; fi
   status=`expr $status + $ret`
fi

echo "I:checking non-cachable NODATA response handling"
ret=0
$DIG +tcp nodata.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking non-cachable NODATA response handling using dns_client"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 nodata.example.net 2> resolve.out || ret=1
    grep "resolution failed: ncache nxrrset" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking handling of bogus referrals"
# If the server has the "INSIST(!external)" bug, this query will kill it.
$DIG +tcp www.example.com. a @10.53.0.1 -p 5300 >/dev/null || status=1

if [ -x ${RESOLVE} ] ; then
    echo "I:checking handling of bogus referrals using dns_client"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 www.example.com 2> resolve.out || ret=1
    grep "resolution failed: failure" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:check handling of cname + other data / 1"
$DIG +tcp cname1.example.com. a @10.53.0.1 -p 5300 >/dev/null || status=1

echo "I:check handling of cname + other data / 2"
$DIG +tcp cname2.example.com. a @10.53.0.1 -p 5300 >/dev/null || status=1

echo "I:check that server is still running"
$DIG +tcp www.example.com. a @10.53.0.1 -p 5300 >/dev/null || status=1

echo "I:checking answer IPv4 address filtering (deny)"
ret=0
$DIG +tcp www.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: SERVFAIL" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking answer IPv6 address filtering (deny)"
ret=0
$DIG +tcp www.example.net @10.53.0.1 aaaa -p 5300 > dig.out || ret=1
grep "status: SERVFAIL" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking answer IPv4 address filtering (accept)"
ret=0
$DIG +tcp www.example.org @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


if [ -x ${RESOLVE} ] ; then
    echo "I:checking answer IPv4 address filtering using dns_client (accept)"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 www.example.org > resolve.out || ret=1
    grep "www.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking answer IPv6 address filtering (accept)"
ret=0
$DIG +tcp www.example.org @10.53.0.1 aaaa -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking answer IPv6 address filtering using dns_client (accept)"
    ret=0
    ${RESOLVE} -p 5300 -t aaaa -s 10.53.0.1 www.example.org > resolve.out || ret=1
    grep "www.example.org..*.2001:db8:beef::1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking CNAME target filtering (deny)"
ret=0
$DIG +tcp badcname.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: SERVFAIL" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking CNAME target filtering (accept)"
ret=0
$DIG +tcp goodcname.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking CNAME target filtering using dns_client (accept)"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 goodcname.example.net > resolve.out || ret=1
    grep "goodcname.example.net..*.goodcname.example.org." resolve.out > /dev/null || ret=1
    grep "goodcname.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking CNAME target filtering (accept due to subdomain)"
ret=0
$DIG +tcp cname.sub.example.org @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking CNAME target filtering using dns_client (accept due to subdomain)"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 cname.sub.example.org > resolve.out || ret=1
    grep "cname.sub.example.org..*.ok.sub.example.org." resolve.out > /dev/null || ret=1
    grep "ok.sub.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking DNAME target filtering (deny)"
ret=0
$DIG +tcp foo.baddname.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: SERVFAIL" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking DNAME target filtering (accept)"
ret=0
$DIG +tcp foo.gooddname.example.net @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking DNAME target filtering using dns_client (accept)"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 foo.gooddname.example.net > resolve.out || ret=1
    grep "foo.gooddname.example.net..*.gooddname.example.org" resolve.out > /dev/null || ret=1
    grep "foo.gooddname.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

echo "I:checking DNAME target filtering (accept due to subdomain)"
ret=0
$DIG +tcp www.dname.sub.example.org @10.53.0.1 a -p 5300 > dig.out || ret=1
grep "status: NOERROR" dig.out > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if [ -x ${RESOLVE} ] ; then
    echo "I:checking DNAME target filtering using dns_client (accept due to subdomain)"
    ret=0
    ${RESOLVE} -p 5300 -t a -s 10.53.0.1 www.dname.sub.example.org > resolve.out || ret=1
    grep "www.dname.sub.example.org..*.ok.sub.example.org." resolve.out > /dev/null || ret=1
    grep "www.ok.sub.example.org..*.192.0.2.1" resolve.out > /dev/null || ret=1
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

n=`expr $n + 1`
echo "I: RT21594 regression test check setup ($n)"
ret=0
# Check that "aa" is not being set by the authoritative server.
$DIG +tcp . @10.53.0.4 soa -p 5300 > dig.ns4.out.${n} || ret=1
grep 'flags: qr rd;' dig.ns4.out.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: RT21594 regression test positive answers ($n)"
ret=0
# Check that resolver accepts the non-authoritative positive answers.
$DIG +tcp . @10.53.0.5 soa -p 5300 > dig.ns5.out.${n} || ret=1
grep "status: NOERROR" dig.ns5.out.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: RT21594 regression test NODATA answers ($n)"
ret=0
# Check that resolver accepts the non-authoritative nodata answers.
$DIG +tcp . @10.53.0.5 txt -p 5300 > dig.ns5.out.${n} || ret=1
grep "status: NOERROR" dig.ns5.out.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: RT21594 regression test NXDOMAIN answers ($n)"
ret=0
# Check that resolver accepts the non-authoritative positive answers.
$DIG +tcp noexistant @10.53.0.5 txt -p 5300 > dig.ns5.out.${n} || ret=1
grep "status: NXDOMAIN" dig.ns5.out.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check that replacement of additional data by a negative cache no data entry clears the additional RRSIGs ($n)"
ret=0
$DIG +tcp mx example.net @10.53.0.7 -p 5300 > dig.ns7.out.${n} || ret=1
grep "status: NOERROR" dig.ns7.out.${n} > /dev/null || ret=1
if [ $ret = 1 ]; then echo "I:mx priming failed"; fi
$NSUPDATE << EOF
server 10.53.0.6 5300
zone example.net
update delete mail.example.net A
update add mail.example.net 0 AAAA ::1
send
EOF
$DIG +tcp a mail.example.net @10.53.0.7 -p 5300 > dig.ns7.out.${n} || ret=2
grep "status: NOERROR" dig.ns7.out.${n} > /dev/null || ret=2
grep "ANSWER: 0" dig.ns7.out.${n} > /dev/null || ret=2
if [ $ret = 2 ]; then echo "I:ncache priming failed"; fi
$DIG +tcp mx example.net @10.53.0.7 -p 5300 > dig.ns7.out.${n} || ret=3
grep "status: NOERROR" dig.ns7.out.${n} > /dev/null || ret=3
$DIG +tcp rrsig mail.example.net +norec @10.53.0.7 -p 5300 > dig.ns7.out.${n}  || ret=4
grep "status: NOERROR" dig.ns7.out.${n} > /dev/null || ret=4
grep "ANSWER: 0" dig.ns7.out.${n} > /dev/null || ret=4
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that update a nameservers address has immediate effects ($n)"
ret=0
$DIG +tcp TXT foo.moves @10.53.0.7 -p 5300 > dig.ns7.foo.${n} || ret=1
grep "From NS 5" dig.ns7.foo.${n} > /dev/null || ret=1 
$NSUPDATE << EOF
server 10.53.0.7 5300
zone server
update delete ns.server A
update add ns.server 300 A 10.53.0.4
send
EOF
sleep 1
$DIG +tcp TXT bar.moves @10.53.0.7 -p 5300 > dig.ns7.bar.${n} || ret=1
grep "From NS 4" dig.ns7.bar.${n} > /dev/null || ret=1

if [ $ret != 0 ]; then echo "I:failed"; status=1; fi

n=`expr $n + 1`
echo "I:checking that update a nameservers glue has immediate effects ($n)"
ret=0
$DIG +tcp TXT foo.child.server @10.53.0.7 -p 5300 > dig.ns7.foo.${n} || ret=1
grep "From NS 5" dig.ns7.foo.${n} > /dev/null || ret=1 
$NSUPDATE << EOF
server 10.53.0.7 5300
zone server
update delete ns.child.server A
update add ns.child.server 300 A 10.53.0.4
send
EOF
sleep 1
$DIG +tcp TXT bar.child.server @10.53.0.7 -p 5300 > dig.ns7.bar.${n} || ret=1
grep "From NS 4" dig.ns7.bar.${n} > /dev/null || ret=1

if [ $ret != 0 ]; then echo "I:failed"; status=1; fi

n=`expr $n + 1`
echo "I:checking empty RFC 1918 reverse zones ($n)"
ret=0
# Check that "aa" is being set by the resolver for RFC 1918 zones
# except the one that has been deliberately disabled
$DIG @10.53.0.7 -p 5300 -x 10.1.1.1 > dig.ns4.out.1.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.1.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 192.168.1.1 > dig.ns4.out.2.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.2.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.16.1.1  > dig.ns4.out.3.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.3.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.17.1.1 > dig.ns4.out.4.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.4.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.18.1.1 > dig.ns4.out.5.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.5.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.19.1.1 > dig.ns4.out.6.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.6.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.21.1.1 > dig.ns4.out.7.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.7.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.22.1.1 > dig.ns4.out.8.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.8.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.23.1.1 > dig.ns4.out.9.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.9.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.24.1.1 > dig.ns4.out.11.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.11.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.25.1.1 > dig.ns4.out.12.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.12.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.26.1.1 > dig.ns4.out.13.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.13.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.27.1.1 > dig.ns4.out.14.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.14.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.28.1.1 > dig.ns4.out.15.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.15.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.29.1.1 > dig.ns4.out.16.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.16.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.30.1.1 > dig.ns4.out.17.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.17.${n} > /dev/null || ret=1
$DIG @10.53.0.7 -p 5300 -x 172.31.1.1 > dig.ns4.out.18.${n} || ret=1
grep 'flags: qr aa rd ra;' dig.ns4.out.18.${n} > /dev/null || ret=1
# but this one should NOT be authoritative
$DIG @10.53.0.7 -p 5300 -x 172.20.1.1 > dig.ns4.out.19.${n} || ret=1
grep 'flags: qr rd ra;' dig.ns4.out.19.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; status=1; fi

n=`expr $n + 1`
echo "I:checking that removal of a delegation is honoured ($n)"
ret=0
$DIG -p 5300 @10.53.0.5 www.to-be-removed.tld A > dig.ns5.prime.${n}
grep "status: NOERROR" dig.ns5.prime.${n} > /dev/null || { ret=1; echo "I: priming failed"; }
cp ns4/tld2.db ns4/tld.db
($RNDC -c ../common/rndc.conf -s 10.53.0.4 -p 9953 reload tld 2>&1 ) | 
sed -e '/reload queued/d' -e 's/^/I:ns4 /'
old=
for i in 0 1 2 3 4 5 6 7 8 9
do
	foo=0
	$DIG -p 5300 @10.53.0.5 ns$i.to-be-removed.tld A > /dev/null
	$DIG -p 5300 @10.53.0.5 www.to-be-removed.tld A > dig.ns5.out.${n}
	grep "status: NXDOMAIN" dig.ns5.out.${n} > /dev/null || foo=1
	[ $foo = 0 ] && break
	$NSUPDATE << EOF
server 10.53.0.6 5300
zone to-be-removed.tld
update add to-be-removed.tld 100 NS ns${i}.to-be-removed.tld
update delete to-be-removed.tld NS ns${old}.to-be-removed.tld
send
EOF
	old=$i
	sleep 1
done
[ $ret = 0 ] && ret=$foo; 
if [ $ret != 0 ]; then echo "I:failed"; status=1; fi

n=`expr $n + 1`
echo "I:check for improved error message with SOA mismatch ($n)"
ret=0
$DIG @10.53.0.1 -p 5300 www.sub.broken aaaa > dig.out.${n} || ret=1
grep "not subdomain of zone" ns1/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

cp ns7/named2.conf ns7/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.7 -p 9953 reconfig 2>&1 | sed 's/^/I:ns7 /'

n=`expr $n + 1`
echo "I:check resolution on the listening port ($n)"
ret=0
$DIG +tcp +tries=2 +time=5 mx example.net @10.53.0.7 -p 5300 > dig.ns7.out.${n} || ret=2
grep "status: NOERROR" dig.ns7.out.${n} > /dev/null || ret=1
grep "ANSWER: 1" dig.ns7.out.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check prefetch (${n})"
ret=0
$DIG @10.53.0.5 -p 5300 fetch.tld txt > dig.out.1.${n} || ret=1
ttl1=`awk '/"A" "short" "ttl"/ { print $2 - 2 }' dig.out.1.${n}`
# sleep so we are in prefetch range
sleep ${ttl1:-0}
# trigger prefetch
$DIG @10.53.0.5 -p 5300 fetch.tld txt > dig.out.2.${n} || ret=1
ttl2=`awk '/"A" "short" "ttl"/ { print $2 }' dig.out.2.${n}`
sleep 1
# check that prefetch occured
$DIG @10.53.0.5 -p 5300 fetch.tld txt > dig.out.3.${n} || ret=1
ttl=`awk '/"A" "short" "ttl"/ { print $2 }' dig.out.3.${n}`
test ${ttl:-0} -gt ${ttl2:-1} || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check prefetch disabled (${n})"
ret=0
$DIG @10.53.0.7 -p 5300 fetch.example.net txt > dig.out.1.${n} || ret=1
ttl1=`awk '/"A" "short" "ttl"/ { print $2 - 1 }' dig.out.1.${n}`
# sleep so we are in expire range
sleep ${ttl1:-0}
# look for zero ttl, allow for one miss at getting zero ttl
zerotonine="0 1 2 3 4 5 6 7 8 9"
for i in $zerotonine $zerotonine $zerotonine $zerotonine
do 
	$DIG @10.53.0.7 -p 5300 fetch.example.net txt > dig.out.2.${n} || ret=1
	ttl2=`awk '/"A" "short" "ttl"/ { print $2 }' dig.out.2.${n}`
	test ${ttl2:-1} -eq 0 && break
	$PERL -e 'select(undef, undef, undef, 0.05);' 
done
test ${ttl2:-1} -eq 0 || ret=1
# delay so that any prefetched record will have a lower ttl than expected
sleep 3
# check that prefetch has not occured
$DIG @10.53.0.7 -p 5300 fetch.example.net txt > dig.out.3.${n} || ret=1
ttl=`awk '/"A" "short" "ttl"/ { print $2 - 1 }' dig.out.3.${n}`
test ${ttl:-0} -eq ${ttl1:-1} || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check prefetch qtype * (${n})"
ret=0
$DIG @10.53.0.5 -p 5300 fetchall.tld any > dig.out.1.${n} || ret=1
ttl1=`awk '/"A" "short" "ttl"/ { print $2 - 2 }' dig.out.1.${n}`
# sleep so we are in prefetch range
sleep ${ttl1:-0}
# trigger prefetch
$DIG @10.53.0.5 -p 5300 fetchall.tld any > dig.out.2.${n} || ret=1
ttl2=`awk '/"A" "short" "ttl"/ { print $2 }' dig.out.2.${n}`
sleep 1
# check that the nameserver is still alive
$DIG @10.53.0.5 -p 5300 fetchall.tld any > dig.out.3.${n} || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check that E was logged on EDNS queries in the query log (${n})"
ret=0
$DIG @10.53.0.5 -p 5300 +edns edns.fetchall.tld any > dig.out.2.${n} || ret=1
grep "query: edns.fetchall.tld IN ANY +E" ns5/named.run > /dev/null || ret=1
$DIG @10.53.0.5 -p 5300 +noedns noedns.fetchall.tld any > dig.out.2.${n} || ret=1
grep "query: noedns.fetchall.tld IN ANY" ns5/named.run > /dev/null || ret=1
grep "query: noedns.fetchall.tld IN ANY +E" ns5/named.run > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check that '-t aaaa' in .digrc does not have unexpected side effects ($n)"
ret=0
echo "-t aaaa" > .digrc
env HOME=`pwd` $DIG @10.53.0.4 -p 5300 . > dig.out.1.${n} || ret=1
env HOME=`pwd` $DIG @10.53.0.4 -p 5300 . A > dig.out.2.${n} || ret=1
env HOME=`pwd` $DIG @10.53.0.4 -p 5300 -x 127.0.0.1 > dig.out.3.${n} || ret=1
grep ';\..*IN.*AAAA$' dig.out.1.${n} > /dev/null || ret=1
grep ';\..*IN.*A$' dig.out.2.${n} > /dev/null || ret=1
grep 'extra type option' dig.out.2.${n} > /dev/null && ret=1
grep ';1\.0\.0\.127\.in-addr\.arpa\..*IN.*PTR$' dig.out.3.${n} > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check that CNAME nameserver is logged correctly (${n})"
ret=0
$DIG soa all-cnames @10.53.0.5 -p 5300 > dig.out.ns5.test${n} || ret=1
grep "status: SERVFAIL" dig.out.ns5.test${n} > /dev/null || ret=1
grep "skipping nameserver 'cname.tld' because it is a CNAME, while resolving 'all-cnames/SOA'" ns5/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
