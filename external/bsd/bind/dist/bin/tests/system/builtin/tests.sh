# Copyright (C) 2011, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.3 2011/08/09 04:12:25 tbox Exp 

status=0
n=0

n=`expr $n + 1`
echo "I:Checking that reconfiguring empty zones is silent ($n)"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reconfig
ret=0
grep "automatic empty zone" ns1/named.run > /dev/null || ret=1
grep "received control channel command 'reconfig'" ns1/named.run > /dev/null || ret=1
grep "reloading configuration succeeded" ns1/named.run > /dev/null || ret=1
sleep 1
grep "zone serial (0) unchanged." ns1/named.run > /dev/null && ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
echo "I:Checking that reloading empty zones is silent ($n)"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload > /dev/null
ret=0
grep "automatic empty zone" ns1/named.run > /dev/null || ret=1
grep "received control channel command 'reload'" ns1/named.run > /dev/null || ret=1
grep "reloading configuration succeeded" ns1/named.run > /dev/null || ret=1
sleep 1
grep "zone serial (0) unchanged." ns1/named.run > /dev/null && ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

VERSION=`../../../../isc-config.sh  --version | cut -d = -f 2`
HOSTNAME=`./gethostname`

n=`expr $n + 1`
ret=0
echo "I:Checking that default version works for rndc ($n)"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 status > rndc.status.ns1.$n 2>&1
grep "^version: $VERSION " rndc.status.ns1.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that custom version works for rndc ($n)"
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 status > rndc.status.ns3.$n 2>&1
grep "^version: $VERSION (this is a test of version) " rndc.status.ns3.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that default version works for query ($n)"
$DIG +short version.bind txt ch @10.53.0.1 -p 5300 > dig.out.ns1.$n
grep "^\"$VERSION\"$" dig.out.ns1.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that custom version works for query ($n)"
$DIG +short version.bind txt ch @10.53.0.3 -p 5300 > dig.out.ns3.$n
grep "^\"this is a test of version\"$" dig.out.ns3.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that default hostname works for query ($n)"
$DIG +short hostname.bind txt ch @10.53.0.1 -p 5300 > dig.out.ns1.$n
grep "^\"$HOSTNAME\"$" dig.out.ns1.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that custom hostname works for query ($n)"
$DIG +short hostname.bind txt ch @10.53.0.3 -p 5300 > dig.out.ns3.$n
grep "^\"this.is.a.test.of.hostname\"$" dig.out.ns3.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that default server-id is none for query ($n)"
$DIG id.server txt ch @10.53.0.1 -p 5300 > dig.out.ns1.$n
grep "status: NOERROR" dig.out.ns1.$n > /dev/null || ret=1
grep "ANSWER: 0" dig.out.ns1.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that server-id hostname works for query ($n)"
$DIG +short id.server txt ch @10.53.0.2 -p 5300 > dig.out.ns2.$n
grep "^\"$HOSTNAME\"$" dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that server-id hostname works for EDNS name server ID request ($n)"
$DIG +norec +nsid foo @10.53.0.2 -p 5300 > dig.out.ns2.$n
grep "^; NSID: .* (\"$HOSTNAME\")$" dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that custom server-id works for query ($n)"
$DIG +short id.server txt ch @10.53.0.3 -p 5300 > dig.out.ns3.$n
grep "^\"this.is.a.test.of.server-id\"$" dig.out.ns3.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

n=`expr $n + 1`
ret=0
echo "I:Checking that custom server-id works for EDNS name server ID request ($n)"
$DIG +norec +nsid foo @10.53.0.3 -p 5300 > dig.out.ns3.$n
grep "^; NSID: .* (\"this.is.a.test.of.server-id\")$" dig.out.ns3.$n > /dev/null || ret=1
if [ $ret != 0 ] ; then echo I:failed; status=`expr $status + $ret`; fi

exit $status
