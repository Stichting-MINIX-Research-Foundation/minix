#!/bin/sh
#
# Copyright (C) 2004, 2007, 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.22 2012/02/03 23:46:58 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
echo "I:waiting for nameserver to load"
for i in 0 1 2 3 4 5 6 7 8 9
do
	ret=0
	for zone in . example1 e.example1 example2 10.10.10.in-addr.arpa \
	    ip6.int ip6.arpa
	do
		$DIG +tcp -p 5300 @10.53.0.1 soa $zone > dig.out
		grep "status: NOERROR" dig.out > /dev/null || ret=1
		grep "ANSWER: 1," dig.out > /dev/null || ret=1
	done
	test $ret = 0 && break
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:using resolv.conf"
ret=0
for i in 0 1 2 3 4 5 6 7 8 9 
do
	grep ' running$' lwresd1/lwresd.run > /dev/null && break
	sleep 1
done
./lwtest || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

$PERL $SYSTEMTESTTOP/stop.pl . lwresd1

mv lwresd1/lwresd.run lwresd1/lwresd.run.resolv

$PERL $SYSTEMTESTTOP/start.pl . lwresd1 -- "-m record,size,mctx -c lwresd.conf -d 99 -g"

echo "I:using lwresd.conf"
ret=0
for i in 0 1 2 3 4 5 6 7 8 9 
do
	grep ' running$' lwresd1/lwresd.run > /dev/null && break
	sleep 1
done
./lwtest || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
