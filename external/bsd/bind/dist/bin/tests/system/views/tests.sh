#!/bin/sh
#
# Copyright (C) 2004, 2007, 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.30 2007/06/19 23:47:06 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:fetching a.example from ns2's initial configuration"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	a.example. @10.53.0.2 any -p 5300 > dig.out.ns2.1 || status=1
grep ";" dig.out.ns2.1	# XXXDCL why is this here?

echo "I:fetching a.example from ns3's initial configuration"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	a.example. @10.53.0.3 any -p 5300 > dig.out.ns3.1 || status=1
grep ";" dig.out.ns3.1	# XXXDCL why is this here?

echo "I:copying in new configurations for ns2 and ns3"
rm -f ns2/named.conf ns3/named.conf ns2/example.db
cp -f ns2/named2.conf ns2/named.conf
cp -f ns3/named2.conf ns3/named.conf
cp -f ns2/example2.db ns2/example.db

echo "I:reloading ns2 and ns3 with rndc"
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 | sed 's/^/I:ns3 /'

echo "I:sleeping for 20 seconds"
sleep 20

echo "I:fetching a.example from ns2's 10.53.0.4, source address 10.53.0.4"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	-b 10.53.0.4 a.example. @10.53.0.4 any -p 5300 > dig.out.ns4.2 \
	|| status=1
grep ";" dig.out.ns4.2	# XXXDCL why is this here?

echo "I:fetching a.example from ns2's 10.53.0.2, source address 10.53.0.2"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	-b 10.53.0.2 a.example. @10.53.0.2 any -p 5300 > dig.out.ns2.2 \
	|| status=1
grep ";" dig.out.ns2.2	# XXXDCL why is this here?

echo "I:fetching a.example from ns3's 10.53.0.3, source address defaulted"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	@10.53.0.3 a.example. any -p 5300 > dig.out.ns3.2 || status=1
grep ";" dig.out.ns3.2	# XXXDCL why is this here?

echo "I:comparing ns3's initial a.example to one from reconfigured 10.53.0.2"
$PERL ../digcomp.pl dig.out.ns3.1 dig.out.ns2.2 || status=1

echo "I:comparing ns3's initial a.example to one from reconfigured 10.53.0.3"
$PERL ../digcomp.pl dig.out.ns3.1 dig.out.ns3.2 || status=1

echo "I:comparing ns2's initial a.example to one from reconfigured 10.53.0.4"
$PERL ../digcomp.pl dig.out.ns2.1 dig.out.ns4.2 || status=1

echo "I:comparing ns2's initial a.example to one from reconfigured 10.53.0.3"
echo "I:(should be different)"
if $PERL ../digcomp.pl dig.out.ns2.1 dig.out.ns3.2 >/dev/null
then
	echo "I:no differences found.  something's wrong."
	status=1
fi

echo "I:updating cloned zone in internal view"
$NSUPDATE << EOF
server 10.53.0.2 5300
zone clone
update add b.clone. 300 in a 10.1.0.3
send
EOF
echo "I:sleeping to allow update to take effect"
sleep 5

echo "I:verifying update affected both views"
ret=0
one=`$DIG +tcp +short -p 5300 -b 10.53.0.2 @10.53.0.2 b.clone a`
two=`$DIG +tcp +short -p 5300 -b 10.53.0.4 @10.53.0.2 b.clone a`
if [ "$one" != "$two" ]; then
        echo "'$one' does not match '$two'"
        ret=1
fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:verifying forwarder in cloned zone works"
ret=0
one=`$DIG +tcp +short -p 5300 -b 10.53.0.2 @10.53.0.2 child.clone txt`
two=`$DIG +tcp +short -p 5300 -b 10.53.0.4 @10.53.0.2 child.clone txt`
three=`$DIG +tcp +short -p 5300 @10.53.0.3 child.clone txt`
four=`$DIG +tcp +short -p 5300 @10.53.0.5 child.clone txt`
echo "$three" | grep NS3 > /dev/null || { ret=1; echo "expected response from NS3 got '$three'"; }
echo "$four" | grep NS5 > /dev/null || { ret=1; echo "expected response from NS5 got '$four'"; }
if [ "$one" = "$two" ]; then
        echo "'$one' matches '$two'"
        ret=1
fi
if [ "$one" != "$three" ]; then
        echo "'$one' does not match '$three'"
        ret=1
fi
if [ "$two" != "$four" ]; then
        echo "'$two' does not match '$four'"
        ret=1
fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

if $SHELL ../testcrypto.sh
then
	echo "I:verifying inline zones work with views"
	ret=0
	$DIG @10.53.0.2 -p 5300 -b 10.53.0.2 +dnssec DNSKEY inline > dig.out.internal
	$DIG @10.53.0.2 -p 5300 -b 10.53.0.5 +dnssec DNSKEY inline > dig.out.external
	grep "ANSWER: 4," dig.out.internal > /dev/null || ret=1
	grep "ANSWER: 4," dig.out.external > /dev/null || ret=1
	int=`awk '$4 == "DNSKEY" { print $8 }' dig.out.internal | sort`
	ext=`awk '$4 == "DNSKEY" { print $8 }' dig.out.external | sort`
	test "$int" != "$ext" || ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
fi

echo "I:exit status: $status"
exit $status
