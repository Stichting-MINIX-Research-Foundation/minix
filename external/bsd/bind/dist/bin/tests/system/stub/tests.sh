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

# Id: tests.sh,v 1.16 2011/11/02 23:46:24 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
echo "I:check that the stub zone has been saved to disk"
for i in 1 2 3 4 5 6 7 8 9 20
do
	[ -f ns3/child.example.st ] && break
	sleep 1
done
[ -f ns3/child.example.st ] || { status=1;  echo "I:failed"; }

for pass in 1 2
do

echo "I:trying an axfr that should be denied (NOTAUTH) (pass=$pass)"
ret=0
$DIG +tcp child.example. @10.53.0.3 axfr -p 5300 > dig.out.ns3 || ret=1
grep "; Transfer failed." dig.out.ns3 > /dev/null || ret=1
[ $ret = 0 ] || { status=1;  echo "I:failed"; }

echo "I:look for stub zone data without recursion (should not be found) (pass=$pass)"
for i in 1 2 3 4 5 6 7 8 9
do
	ret=0
	$DIG +tcp +norec data.child.example. \
		@10.53.0.3 txt -p 5300 > dig.out.ns3 || ret=1
	grep "status: NOERROR" dig.out.ns3 > /dev/null || ret=1
	[ $ret = 0 ] && break
	sleep 1
done
$PERL ../digcomp.pl knowngood.dig.out.norec dig.out.ns3 || ret=1
[ $ret = 0 ] || { status=1;  echo "I:failed"; }

echo "I:look for stub zone data with recursion (should be found) (pass=$pass)"
ret=0
$DIG +tcp +noauth +noadd data.child.example. @10.53.0.3 txt -p 5300 > dig.out.ns3 || ret=1
$PERL ../digcomp.pl knowngood.dig.out.rec dig.out.ns3 || ret=1
[ $ret = 0 ] || { status=1;  echo "I:failed"; }

[ $pass = 1 ] && {
	echo "I:stopping stub server"
	$PERL $SYSTEMTESTTOP/stop.pl . ns3

	echo "I:re-starting stub server"
	$PERL $SYSTEMTESTTOP/start.pl --noclean --restart . ns3
}
done

echo "I:exit status: $status"
exit $status
