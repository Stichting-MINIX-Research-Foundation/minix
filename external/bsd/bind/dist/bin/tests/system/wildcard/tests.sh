#!/bin/sh
#
# Copyright (C) 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.1.2.3 2010/06/01 06:57:31 marka Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"

n=`expr $n + 1`
echo "I: checking that NSEC wildcard non-existance proof is returned auth ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec +norec @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep -i 'a\.wild\.nsec\..*NSEC.*nsec\..*NSEC'  dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC wildcard non-existance proof is returned non-validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep -i 'a\.wild\.nsec\..*NSEC.*nsec\..*NSEC'  dig.out.ns2.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC wildcard non-existance proof is returned validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep -i 'a\.wild\.nsec\..*NSEC.*nsec\..*NSEC'  dig.out.ns3.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC wildcard non-existance proof is returned validating + CD ($n)"
ret=0
$DIG $DIGOPTS +cd a b.wild.nsec @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep -i 'a\.wild\.nsec\..*NSEC.*nsec\..*NSEC'  dig.out.ns5.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns5.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I: checking that returned NSEC wildcard non-existance proof validates ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep -i 'a\.wild\.nsec\..*NSEC.*nsec\..*NSEC'  dig.out.ns4.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC wildcard non-existance proof is returned private, validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.private.nsec @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep -i 'a\.wild\.private\.nsec\..*NSEC.*private\.nsec\..*NSEC'  dig.out.ns3.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that returned NSEC wildcard non-existance proof for private zone validates ($n)"
ret=0
$DIG $DIGOPTS a b.wild.private.nsec @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep -i 'a\.wild\.private\.nsec\..*NSEC.*private\.nsec\..*NSEC'  dig.out.ns4.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC3 wildcard non-existance proof is returned auth ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec3 +norec @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep -i 'O3TJ8D9AJ54CBTFCQCJ3QK49CH7SF6H9\.nsec3\..*V5DLFB6UJNHR94LQ61FO607KGK12H88A'  dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC3 wildcard non-existance proof is returned non-validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec3 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep -i 'O3TJ8D9AJ54CBTFCQCJ3QK49CH7SF6H9\.nsec3\..*V5DLFB6UJNHR94LQ61FO607KGK12H88A'  dig.out.ns2.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC3 wildcard non-existance proof is returned validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep -i 'O3TJ8D9AJ54CBTFCQCJ3QK49CH7SF6H9\.nsec3\..*V5DLFB6UJNHR94LQ61FO607KGK12H88A'  dig.out.ns3.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC3 wildcard non-existance proof is returned validating + CD ($n)"
ret=0
$DIG $DIGOPTS +cd a b.wild.nsec3 @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep -i 'O3TJ8D9AJ54CBTFCQCJ3QK49CH7SF6H9\.nsec3\..*V5DLFB6UJNHR94LQ61FO607KGK12H88A'  dig.out.ns5.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns5.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that returned NSEC3 wildcard non-existance proof validates ($n)"
ret=0
$DIG $DIGOPTS a b.wild.nsec3 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep -i 'O3TJ8D9AJ54CBTFCQCJ3QK49CH7SF6H9\.nsec3\..*V5DLFB6UJNHR94LQ61FO607KGK12H88A'  dig.out.ns4.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that NSEC3 wildcard non-existance proof is returned private, validating ($n)"
ret=0
$DIG $DIGOPTS a b.wild.private.nsec3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep -i 'UDBSP4R8OUOT6HSO39VD8B5LMOSHRD5N\.private\.nsec3\..*NSEC3.*ASDRUIB7GO00OR92S5OUGI404LT27RNU' dig.out.ns3.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I: checking that returned NSEC3 wildcard non-existance proof for private zone validates ($n)"
ret=0
$DIG $DIGOPTS a b.wild.private.nsec3 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep -i 'UDBSP4R8OUOT6HSO39VD8B5LMOSHRD5N\.private\.nsec3\..*NSEC3.*ASDRUIB7GO00OR92S5OUGI404LT27RNU' dig.out.ns4.test$n > /dev/null || ret=1
grep -i 'flags:.* ad[ ;]'  dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
