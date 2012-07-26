# Copyright (C) 2005, 2007, 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.5.114.2 2011-05-07 23:47:05 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I: checking that named-checkconf handles a known good config"
ret=0
$CHECKCONF good.conf > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf prints a known good config"
ret=0
awk 'BEGIN { ok = 0; } /cut here/ { ok = 1; getline } ok == 1 { print }' good.conf > good.conf.in
[ -s good.conf.in ] || ret=1
$CHECKCONF -p good.conf.in | grep -v '^good.conf.in:' > good.conf.out 2>&1 || ret=1
cmp good.conf.in good.conf.out || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking that named-checkconf handles a known bad config"
ret=0
$CHECKCONF bad.conf > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking named-checkconf dnssec warnings"
ret=0
$CHECKCONF dnssec.1 2>&1 | grep 'validation yes.*enable no' > /dev/null || ret=1
$CHECKCONF dnssec.2 2>&1 | grep 'validation auto.*enable no' > /dev/null || ret=1
$CHECKCONF dnssec.2 2>&1 | grep 'validation yes.*enable no' > /dev/null || ret=1
# this one should have no warnings
$CHECKCONF dnssec.3 2>&1 | grep '.*' && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
