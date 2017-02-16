#!/bin/sh
#
# Copyright (C) 2010, 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.3 2010/12/02 23:22:41 marka Exp 

# Test of allow-query statement.
# allow-query takes an address match list and can be included in either the
# options statement or in the zone statement.  This test assumes that the
# acl tests cover the details of the address match list and uses a limited
# number of address match test cases to ensure that allow-query finds the
# expected match.
# Test list:
# In options:
# default (any), any, none, [localhost, localnets], 
# allowed address, not allowed address, denied address,
# allowed key, not allowed key, denied key 
# allowed acl, not allowed acl, denied acl (acls pointing to addresses) 
#
# Each of these tests requires changing to a new configuration
# file and using rndc to update the server
#
# In view, with nothing in options (default to any)
# default (any), any, none, [localhost, localnets], 
# allowed address, not allowed address, denied address,
# allowed key, not allowed key, denied key 
# allowed acl, not allowed acl, denied acl (acls pointing to addresses) 
#
# In view, with options set to none, view set to any
# In view, with options set to any, view set to none
#
# In zone, with nothing in options (default to any)
# any, none, [localhost, localnets], 
# allowed address, denied address, 
# allowed key, not allowed key, denied key
# allowed acl, not allowed acl, denied acl (acls pointing to addresses),
# 
# In zone, with options set to none, zone set to any
# In zone, with options set to any, zone set to none
# In zone, with view set to none, zone set to any
# In zone, with view set to any, zone set to none
#
# zone types of master, slave and stub can be tested in parallel by using
# multiple instances (ns2 as master, ns3 as slave, ns4 as stub) and querying
# as necessary. 
#

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +nosea +nostat +nocmd +norec +noques +noauth +noadd +nostats +dnssec -p 5300"
status=0
n=0

# Test 1 - default, query allowed
n=`expr $n + 1`
echo "I:test $n: default - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 2 - explicit any, query allowed
n=`expr $n + 1`
cp -f ns2/named02.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: explicit any - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 3 - none, query refused
n=`expr $n + 1`
cp -f ns2/named03.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: none - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 4 - address allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named04.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: address allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 5 - address not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named05.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: address not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 6 - address disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named06.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: address disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 7 - acl allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named07.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: acl allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 8 - acl not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named08.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: acl not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


# Test 9 - acl disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named09.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: acl disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 10 - key allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named10.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: key allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 11 - key not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named11.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: key not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y two:1234efgh8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 12 - key disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named12.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: key disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# The next set of tests check if allow-query works in a view

n=20
# Test 21 - views default, query allowed
n=`expr $n + 1`
cp -f ns2/named21.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views default - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 22 - views explicit any, query allowed
n=`expr $n + 1`
cp -f ns2/named22.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views explicit any - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 23 - views none, query refused
n=`expr $n + 1`
cp -f ns2/named23.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views none - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 24 - views address allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named24.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views address allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 25 - views address not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named25.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views address not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 26 - views address disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named26.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views address disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 27 - views acl allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named27.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views acl allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 28 - views acl not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named28.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views acl not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 29 - views acl disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named29.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views acl disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 30 - views key allowed, query allowed
n=`expr $n + 1`
cp -f ns2/named30.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views key allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 31 - views key not allowed, query refused
n=`expr $n + 1`
cp -f ns2/named31.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views key not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y two:1234efgh8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 32 - views key disallowed, query refused
n=`expr $n + 1`
cp -f ns2/named32.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views key disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 33 - views over options, views allow, query allowed
n=`expr $n + 1`
cp -f ns2/named33.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views over options, views allow - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 34 - views over options, views disallow, query refused
n=`expr $n + 1`
cp -f ns2/named34.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views over options, views disallow - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Tests for allow-query in the zone statements

n=40

# Test 41 - zone default, query allowed
n=`expr $n + 1`
cp -f ns2/named40.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: zone default - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 42 - zone explicit any, query allowed
n=`expr $n + 1`
echo "I:test $n: zone explicit any - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.any.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.any.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 43 - zone none, query refused
n=`expr $n + 1`
echo "I:test $n: zone none - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.none.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.none.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 44 - zone address allowed, query allowed
n=`expr $n + 1`
echo "I:test $n: zone address allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.addrallow.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.addrallow.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 45 - zone address not allowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone address not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.addrnotallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.addrnotallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 46 - zone address disallowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone address disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.addrdisallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.addrdisallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 47 - zone acl allowed, query allowed
n=`expr $n + 1`
echo "I:test $n: zone acl allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.aclallow.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.aclallow.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 48 - zone acl not allowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone acl not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.aclnotallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.aclnotallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 49 - zone acl disallowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone acl disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.acldisallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.acldisallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 50 - zone key allowed, query allowed
n=`expr $n + 1`
echo "I:test $n: zone key allowed - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.keyallow.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.keyallow.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 51 - zone key not allowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone key not allowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y two:1234efgh8765 a.keyallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.keyallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 52 - zone key disallowed, query refused
n=`expr $n + 1`
echo "I:test $n: zone key disallowed - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 -y one:1234abcd8765 a.keydisallow.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.keydisallow.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 53 - zones over options, zones allow, query allowed
n=`expr $n + 1`
cp -f ns2/named53.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views over options, views allow - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 54 - zones over options, zones disallow, query refused
n=`expr $n + 1`
cp -f ns2/named54.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: views over options, views disallow - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 55 - zones over views, zones allow, query allowed
n=`expr $n + 1`
cp -f ns2/named55.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: zones over views, views allow - query allowed"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 56 - zones over views, zones disallow, query refused
n=`expr $n + 1`
cp -f ns2/named56.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: zones over views, views disallow - query refused"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test 57 - zones over views, zones disallow, query refused (allow-query-on)
n=`expr $n + 1`
cp -f ns2/named57.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:test $n: zones over views, allow-query-on"
ret=0
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.normal.example a > dig.out.ns2.1.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.1.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.1.$n > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.aclnotallow.example a > dig.out.ns2.2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.2.$n > /dev/null || ret=1
grep '^a.aclnotallow.example' dig.out.ns2.2.$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status

