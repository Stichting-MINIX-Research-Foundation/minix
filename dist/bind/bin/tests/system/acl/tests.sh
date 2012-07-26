#!/bin/sh
#
# Copyright (C) 2008  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.4 2008-07-19 00:02:14 each Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"

status=0
t=0

echo "I:testing basic ACL processing"
# key "one" should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

# any other key should be fine
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

cp -f ns2/named2.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

# prefix 10/8 should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

# any other address should work, as long as it sends key "one"
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 127.0.0.1 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 127.0.0.1 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

echo "I:testing nested ACL processing"
# all combinations of 10.53.0.{1|2} with key {one|two}, should succeed
cp -f ns2/named3.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.2 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.2 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# but only one or the other should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 127.0.0.1 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.2 axfr -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $tt failed" ; status=1; }

# and other values? right out
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 127.0.0.1 axfr -y three:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

# now we only allow 10.53.0.1 *and* key one, or 10.53.0.2 *and* key two
cp -f ns2/named4.conf ns2/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
sleep 5

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.2 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# should succeed
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 && { echo "I:test $t failed" ; status=1; }

# should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.2 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

# should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.1 axfr -y two:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

# should fail
t=`expr $t + 1`
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 -b 10.53.0.3 axfr -y one:1234abcd8765 -p 5300 > dig.out
grep "^;" dig.out > /dev/null 2>&1 || { echo "I:test $t failed" ; status=1; }

echo "I:exit status: $status"
exit $status
