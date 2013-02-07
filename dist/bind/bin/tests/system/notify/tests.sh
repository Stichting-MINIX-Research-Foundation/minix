#!/bin/sh
#
# Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.33 2007-06-19 23:47:04 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.2 a -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.3 a -p 5300 > dig.out.ns3 || status=1
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig.out.ns2 dig.out.ns3 || status=1

rm -f ns2/example.db
cp -f ns2/example2.db ns2/example.db
kill -HUP `cat ns2/named.pid`
sleep 45

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.2 a -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.3 a -p 5300 > dig.out.ns3 || status=1
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig.out.ns2 dig.out.ns3 || status=1

###
# Why does not doing the stop not cause problems with the start further on?
###
$PERL $SYSTEMTESTTOP/stop.pl . ns3

rm -f ns2/example.db
cp -f ns2/example3.db ns2/example.db
kill -HUP `cat ns2/named.pid`
sleep 45

$PERL $SYSTEMTESTTOP/start.pl . ns3

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.2 a -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.3 a -p 5300 > dig.out.ns3 || status=1
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig.out.ns2 dig.out.ns3 || status=1

$PERL $SYSTEMTESTTOP/stop.pl . ns2

rm -f ns2/example.db
cp -f ns2/example4.db ns2/example.db

$PERL $SYSTEMTESTTOP/start.pl . ns2

sleep 45

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.2 a -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd a.example.\
	@10.53.0.3 a -p 5300 > dig.out.ns3 || status=1
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig.out.ns2 dig.out.ns3 || status=1

echo "I:exit status: $status"
exit $status
