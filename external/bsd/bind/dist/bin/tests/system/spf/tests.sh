#!/bin/sh
#
# Copyright (C) 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

n=1
status=0

echo "I:checking that SPF warnings have been correctly generated ($n)"
ret=0

grep "zone spf/IN: loaded serial 0" ns1/named.run > /dev/null || ret=1
grep "'y.spf' found type SPF" ns1/named.run > /dev/null || ret=1
grep "'spf' found type SPF" ns1/named.run > /dev/null && ret=1

grep "zone warn/IN: loaded serial 0" ns1/named.run > /dev/null || ret=1
grep "'y.warn' found type SPF" ns1/named.run > /dev/null || ret=1
grep "'warn' found type SPF" ns1/named.run > /dev/null && ret=1

grep "zone nowarn/IN: loaded serial 0" ns1/named.run > /dev/null || ret=1
grep "'y.nowarn' found type SPF" ns1/named.run > /dev/null && ret=1
grep "'nowarn' found type SPF" ns1/named.run > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
