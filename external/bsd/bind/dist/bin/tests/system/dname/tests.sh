#!/bin/sh
#
# Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.2 2011/03/18 21:14:19 fdupont Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:checking short dname from authoritative"
ret=0
$DIG a.short-dname.example @10.53.0.2 a -p 5300 > dig.out.ns2.short || ret=1
grep "status: NOERROR" dig.out.ns2.short > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking short dname from recursive"
ret=0
$DIG a.short-dname.example @10.53.0.4 a -p 5300 > dig.out.ns4.short || ret=1
grep "status: NOERROR" dig.out.ns4.short > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking long dname from authoritative"
ret=0
$DIG a.long-dname.example @10.53.0.2 a -p 5300 > dig.out.ns2.long || ret=1
grep "status: NOERROR" dig.out.ns2.long > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking long dname from recursive"
ret=0
$DIG a.long-dname.example @10.53.0.4 a -p 5300 > dig.out.ns4.long || ret=1
grep "status: NOERROR" dig.out.ns4.long > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking (too) long dname from authoritative"
ret=0
$DIG 01234567890123456789012345678901234567890123456789.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.long-dname.example @10.53.0.2 a -p 5300 > dig.out.ns2.toolong || ret=1
grep "status: YXDOMAIN" dig.out.ns2.toolong > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking (too) long dname from recursive"
ret=0
$DIG 01234567890123456789012345678901234567890123456789.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.longlonglonglonglonglonglonglonglonglonglonglonglonglonglong.long-dname.example @10.53.0.4 a -p 5300 > dig.out.ns4.toolong || ret=1
grep "status: YXDOMAIN" dig.out.ns4.toolong > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"

exit $status
