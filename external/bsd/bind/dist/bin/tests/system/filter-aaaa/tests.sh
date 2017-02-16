#!/bin/sh
#
# Copyright (C) 2010, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.4 2012/01/31 23:47:31 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

rm -f dig.out.*

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd -p 5300"

for conf in conf/good*.conf
do
	n=`expr $n + 1`
	echo "I:checking that $conf is accepted ($n)"
	ret=0
	$CHECKCONF "$conf" || ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

for conf in conf/bad*.conf
do
	n=`expr $n + 1`
	echo "I:checking that $conf is rejected ($n)"
	ret=0
	$CHECKCONF "$conf" >/dev/null && ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

#
# Authoritative tests against:
#	filter-aaaa-on-v4 yes;
#	filter-aaaa { 10.53.0.1; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep ::2 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep ::5 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist, signed and DO set ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep ::3 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.2 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep ::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns1.test$n > /dev/null || ret=1
grep "::3" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns1.test$n > /dev/null || ret=1
grep "::6" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, signed, qtype=ANY and DO is set ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep ::3 dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns1.test$n > /dev/null || ret=1
grep "::6" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.2 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns1.test$n > /dev/null || ret=1
grep ::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv6 ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::1
then
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep 2001:db8::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS ($n)"
ret=0
$DIG $DIGOPTS +add ns unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep AAAA dig.out.ns1.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, signed ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv6 ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::1
then
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi


#
# Authoritative tests against:
#	filter-aaaa-on-v4 break-dnssec;
#	filter-aaaa { 10.53.0.4; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep ::2 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep ::5 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed and DO set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.2 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep ::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns4.test$n > /dev/null || ret=1
grep "::3" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns4.test$n > /dev/null || ret=1
grep "::6" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns4.test$n > /dev/null || ret=1
grep ::3 dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns4.test$n > /dev/null || ret=1
grep "::6" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.2 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns4.test$n > /dev/null || ret=1
grep ::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv6 with break-dnssec ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::4
then
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep 2001:db8::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add ns unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep AAAA dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns4.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, signed, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv6, with break-dnssec ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::4
then
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi


#
# Recursive tests against:
#	filter-aaaa-on-v4 yes;
#	filter-aaaa { 10.53.0.2; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep ::2 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep ::5 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist, signed and DO set, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep ::3 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned +dnssec -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.1 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep ::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns2.test$n > /dev/null || ret=1
grep "::3" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns2.test$n > /dev/null || ret=1
grep "::6" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, signed, qtype=ANY and DO is set, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep ::3 dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns2.test$n > /dev/null || ret=1
grep "::6" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.1 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns2.test$n > /dev/null || ret=1
grep ::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv6, recursive ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::2
then
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep 2001:db8::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS ($n)"
ret=0
$DIG $DIGOPTS +add ns unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep AAAA dig.out.ns2.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, signed ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv6 ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::2
then
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi


#
# Recursive tests against:
#	filter-aaaa-on-v4 break-dnssec;
#	filter-aaaa { 10.53.0.3; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep ::2 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep ::5 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed and DO set, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned +dnssec -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.1 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep ::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns3.test$n > /dev/null || ret=1
grep "::3" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns3.test$n > /dev/null || ret=1
grep "::6" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns3.test$n > /dev/null || ret=1
grep ::3 dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns3.test$n > /dev/null || ret=1
grep "::6" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b 10.53.0.1 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns3.test$n > /dev/null || ret=1
grep ::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv6, recursive with break-dnssec ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::3
then
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep 2001:db8::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add ns unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep AAAA dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns3.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv6, recursive with break-dnssec ($n)"
if $TESTSOCK6 fd92:7065:b8e:ffff::3
then
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
else
echo "I: skipped."
fi

$TESTSOCK6 fd92:7065:b8e:ffff::1 || {
        echo "I:IPv6 address not configured; skipping IPv6 query tests"
        echo "I:exit status: $status"
        exit $status
}

# Reconfiguring for IPv6 tests
echo "I:reconfiguring servers"
cp -f ns1/named2.conf ns1/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reconfig 2>&1 | sed 's/^/I:ns1 /'
cp -f ns2/named2.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reconfig 2>&1 | sed 's/^/I:ns2 /'
cp -f ns3/named2.conf ns3/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reconfig 2>&1 | sed 's/^/I:ns3 /'
cp -f ns4/named2.conf ns4/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.4 -p 9953 reconfig 2>&1 | sed 's/^/I:ns4 /'

# BEGIN IPv6 TESTS

#
# Authoritative tests against:
#	filter-aaaa-on-v6 yes;
#	filter-aaaa { fd92:7065:b8e:ffff::1; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep ::2 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep ::5 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist, signed and DO set ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep ::3 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "ANSWER: 0" dig.out.ns1.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep ::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns1.test$n > /dev/null || ret=1
grep "::3" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns1.test$n > /dev/null || ret=1
grep "::6" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, signed, qtype=ANY and DO is set ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep ::3 dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns1.test$n > /dev/null || ret=1
grep "::6" dig.out.ns1.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "status: NOERROR" dig.out.ns1.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns1.test$n > /dev/null || ret=1
grep ::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv4 ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 2001:db8::6 dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec ns unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep AAAA dig.out.ns1.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, signed ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::1 > dig.out.ns1.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv4 ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.1 @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns1.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


#
# Authoritative tests against:
#	filter-aaaa-on-v6 break-dnssec;
#	filter-aaaa { fd92:7065:b8e:ffff::4; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep ::2 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep ::5 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed and DO set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep ::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns4.test$n > /dev/null || ret=1
grep "::3" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns4.test$n > /dev/null || ret=1
grep "::6" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns4.test$n > /dev/null || ret=1
grep ::3 dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns4.test$n > /dev/null || ret=1
grep "::6" dig.out.ns4.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns4.test$n > /dev/null || ret=1
grep ::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv4 with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep 2001:db8::6 dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec ns unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep AAAA dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns4.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, signed, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b fd92:7065:b8e:ffff::4 @fd92:7065:b8e:ffff::4 > dig.out.ns4.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv4, with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.4 @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns4.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


#
# Recursive tests against:
#	filter-aaaa-on-v6 yes;
#	filter-aaaa { fd92:7065:b8e:ffff::2; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep ::2 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep ::5 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist, signed and DO set, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep ::3 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned +dnssec -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "ANSWER: 0" dig.out.ns2.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep ::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns2.test$n > /dev/null || ret=1
grep "::3" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns2.test$n > /dev/null || ret=1
grep "::6" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, signed, qtype=ANY and DO is set, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep ::3 dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns2.test$n > /dev/null || ret=1
grep "::6" dig.out.ns2.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl, recursive ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns2.test$n > /dev/null || ret=1
grep ::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv4, recursive ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep 2001:db8::6 dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec ns unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep AAAA dig.out.ns2.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, signed ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b fd92:7065:b8e:ffff::2 @fd92:7065:b8e:ffff::2 > dig.out.ns2.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv4 ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.2 @10.53.0.2 > dig.out.ns2.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns2.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


#
# Recursive tests against:
#	filter-aaaa-on-v6 yes;
#	filter-aaaa { fd92:7065:b8e:ffff::3; };
#
n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.signed -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep ::2 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when only AAAA record exists, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa aaaa-only.unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep ::5 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, signed and DO set, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.signed +dnssec -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that NODATA/NOERROR is returned when both AAAA and A records exist, unsigned and DO set, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned +dnssec -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "ANSWER: 0" dig.out.ns3.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A records exist and query source does not match acl, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep ::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns3.test$n > /dev/null || ret=1
grep "::3" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned and qtype=ANY with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns3.test$n > /dev/null || ret=1
grep "::6" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, signed, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.signed +dnssec -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.3" dig.out.ns3.test$n > /dev/null || ret=1
grep ::3 dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that A and not AAAA is returned when both AAAA and A records exist, unsigned, qtype=ANY and DO is set with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned +dnssec -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "1.0.0.6" dig.out.ns3.test$n > /dev/null || ret=1
grep "::6" dig.out.ns3.test$n > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that both A and AAAA are returned when both AAAA and A records exist, qtype=ANY and query source does not match acl, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS any dual.unsigned -b fd92:7065:b8e:ffff::1 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep 1.0.0.6 dig.out.ns3.test$n > /dev/null || ret=1
grep ::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is returned when both AAAA and A record exists, unsigned over IPv4, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS aaaa dual.unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep 2001:db8::6 dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=NS, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec ns unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep AAAA dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
grep "ADDITIONAL: 2" dig.out.ns3.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, unsigned, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is omitted from additional section, qtype=MX, signed, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx signed -b fd92:7065:b8e:ffff::3 @fd92:7065:b8e:ffff::3 > dig.out.ns3.test$n || ret=1
grep "^mx.signed.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that AAAA is included in additional section, qtype=MX, unsigned, over IPv4, recursive with break-dnssec ($n)"
ret=0
$DIG $DIGOPTS +add +dnssec mx unsigned -b 10.53.0.3 @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "^mx.unsigned.*AAAA" dig.out.ns3.test$n > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
