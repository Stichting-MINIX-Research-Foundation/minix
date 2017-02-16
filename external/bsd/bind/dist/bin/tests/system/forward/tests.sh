# Copyright (C) 2004, 2007, 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.9 2011/10/13 22:48:23 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

root=10.53.0.1
hidden=10.53.0.2
f1=10.53.0.3
f2=10.53.0.4

status=0

echo "I:checking that a forward zone overrides global forwarders"
ret=0
$DIG +noadd +noauth txt.example1. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG +noadd +noauth txt.example1. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward first zone no forwarders recurses"
ret=0
$DIG +noadd +noauth txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG +noadd +noauth txt.example2. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward only zone no forwarders fails"
ret=0
$DIG +noadd +noauth txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG +noadd +noauth txt.example2. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that global forwarders work"
ret=0
$DIG +noadd +noauth txt.example4. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG +noadd +noauth txt.example4. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward zone works"
ret=0
$DIG +noadd +noauth txt.example1. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG +noadd +noauth txt.example1. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that forwarding doesn't spontaneously happen"
ret=0
$DIG +noadd +noauth txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG +noadd +noauth txt.example2. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward zone with no specified policy works"
ret=0
$DIG +noadd +noauth txt.example3. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG +noadd +noauth txt.example3. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward only doesn't recurse"
ret=0
$DIG txt.example5. txt @$f2 -p 5300 > dig.out.f2 || ret=1
grep "SERVFAIL" dig.out.f2 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for negative caching of forwarder response"
# prime the cache, shutdown the forwarder then check that we can
# get the answer from the cache.  restart forwarder.
ret=0
$DIG nonexist. txt @10.53.0.5 -p 5300 > dig.out.f2 || ret=1
grep "status: NXDOMAIN" dig.out.f2 > /dev/null || ret=1
$PERL ../stop.pl . ns4 || ret=1
$DIG nonexist. txt @10.53.0.5 -p 5300 > dig.out.f2 || ret=1
grep "status: NXDOMAIN" dig.out.f2 > /dev/null || ret=1
$PERL ../start.pl --restart --noclean . ns4 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that forward only zone overrides empty zone"
ret=0
$DIG 1.0.10.in-addr.arpa TXT @10.53.0.4 -p 5300 > dig.out.f2
grep "status: NOERROR" dig.out.f2 > /dev/null || ret=1
$DIG 2.0.10.in-addr.arpa TXT @10.53.0.4 -p 5300 > dig.out.f2
grep "status: NXDOMAIN" dig.out.f2 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that DS lookups for grafting forward zones are isolated"
ret=0
$DIG grafted A @10.53.0.4 -p 5300 > dig.out.q1
$DIG grafted DS @10.53.0.4 -p 5300 > dig.out.q2
$DIG grafted A @10.53.0.4 -p 5300 > dig.out.q3
$DIG grafted AAAA @10.53.0.4 -p 5300 > dig.out.q4
grep "status: NOERROR" dig.out.q1 > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.q2 > /dev/null || ret=1
grep "status: NOERROR" dig.out.q3 > /dev/null || ret=1
grep "status: NOERROR" dig.out.q4 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
