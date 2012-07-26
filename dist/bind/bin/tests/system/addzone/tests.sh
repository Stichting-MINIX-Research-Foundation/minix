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

# $Id: tests.sh,v 1.4.54.2 2011-06-17 23:47:11 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +nosea +nostat +nocmd +norec +noques +noauth +noadd +nostats +dnssec -p 5300"
status=0
n=0

echo "I:checking normally loaded zone ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 a.normal.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking previously added zone ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 a.previous.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.previous.example' dig.out.ns2.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:adding new zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 addzone 'added.example { type master; file "added.db"; };' 2>&1 | sed 's/^/I:ns2 /'
$DIG $DIGOPTS @10.53.0.2 a.added.example a > dig.out.ns2.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.added.example' dig.out.ns2.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:adding new zone with missing master file ($n)"
ret=0
$DIG $DIGOPTS +all @10.53.0.2 a.missing.example a > dig.out.ns2.pre.$n || ret=1
grep "status: REFUSED" dig.out.ns2.pre.$n > /dev/null || ret=1
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 addzone 'missing.example { type master; file "missing.db"; };' 2> rndc.out.ns2.$n
grep "file not found" rndc.out.ns2.$n > /dev/null || ret=1
$DIG $DIGOPTS +all @10.53.0.2 a.missing.example a > dig.out.ns2.post.$n || ret=1
grep "status: REFUSED" dig.out.ns2.post.$n > /dev/null || ret=1
$PERL ../digcomp.pl dig.out.ns2.pre.$n dig.out.ns2.post.$n || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:deleting previously added zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 delzone previous.example 2>&1 | sed 's/^/I:ns2 /'
$DIG $DIGOPTS @10.53.0.2 a.previous.example a > dig.out.ns2.$n
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.previous.example' dig.out.ns2.$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:deleting newly added zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 delzone added.example 2>&1 | sed 's/^/I:ns2 /'
$DIG $DIGOPTS @10.53.0.2 a.added.example a > dig.out.ns2.$n
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.added.example' dig.out.ns2.$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:attempt to delete a normally-loaded zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 delzone normal.example 2> rndc.out.ns2.$n
grep "permission denied" rndc.out.ns2.$n > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.2 a.normal.example a > dig.out.ns2.$n
grep 'status: NOERROR' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.normal.example' dig.out.ns2.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:reconfiguring server with multiple views"
rm -f ns2/named.conf 
cp -f ns2/named2.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reconfig 2>&1 | sed 's/^/I:ns2 /'
sleep 5

echo "I:adding new zone to external view ($n)"
# NOTE: The internal view has "recursion yes" set, and so queries for
# nonexistent zones should return NOERROR.  The external view is
# "recursion no", so queries for nonexistent zones should return
# REFUSED.  This behavior should be the same regardless of whether
# the zone does not exist because a) it has not yet been loaded, b)
# it failed to load, or c) it has been deleted.
ret=0
$DIG +norec $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.added.example a > dig.out.ns2.intpre.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.intpre.$n > /dev/null || ret=1
$DIG +norec $DIGOPTS @10.53.0.4 -b 10.53.0.4 a.added.example a > dig.out.ns2.extpre.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.extpre.$n > /dev/null || ret=1
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 addzone 'added.example in external { type master; file "added.db"; };' 2>&1 | sed 's/^/I:ns2 /'
$DIG +norec $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.added.example a > dig.out.ns2.int.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.int.$n > /dev/null || ret=1
$DIG +norec $DIGOPTS @10.53.0.4 -b 10.53.0.4 a.added.example a > dig.out.ns2.ext.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.ext.$n > /dev/null || ret=1
grep '^a.added.example' dig.out.ns2.ext.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:deleting newly added zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 delzone 'added.example in external' 2>&1 | sed 's/^/I:ns2 /'
$DIG $DIGOPTS @10.53.0.4 -b 10.53.0.4 a.added.example a > dig.out.ns2.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.$n > /dev/null || ret=1
grep '^a.added.example' dig.out.ns2.$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:attempting to add zone to internal view ($n)"
ret=0
$DIG +norec $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.added.example a > dig.out.ns2.pre.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.pre.$n > /dev/null || ret=1
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 addzone 'added.example in internal { type master; file "added.db"; };' 2> rndc.out.ns2.$n
grep "permission denied" rndc.out.ns2.$n > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.2 -b 10.53.0.2 a.added.example a > dig.out.ns2.int.$n || ret=1
grep 'status: NOERROR' dig.out.ns2.int.$n > /dev/null || ret=1
$DIG $DIGOPTS @10.53.0.4 -b 10.53.0.4 a.added.example a > dig.out.ns2.ext.$n || ret=1
grep 'status: REFUSED' dig.out.ns2.ext.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:ensure the configuration context is cleaned up correctly ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reconfig > /dev/null 2>&1 || ret=1
sleep 5
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 status > /dev/null 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
