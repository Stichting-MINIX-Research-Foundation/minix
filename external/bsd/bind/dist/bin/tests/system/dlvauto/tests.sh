# Copyright (C) 2011, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.3 2011/03/03 16:16:46 each Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"

status=0
n=1

#
#  When this was initialy reported there was a REQUIRE failure on restarting.
#
echo "I:checking dnssec-lookaside "'"auto"'"; with views of multiple classes ($n)" 
if [ -s  ns2/named.pid ]
then
	ret=0
	$PERL $SYSTEMTESTTOP/stop.pl . ns2 || ret=1
	sleep 1
	(cd ns2; $NAMED -g -d 100 -c named.conf >> named.run 2>&1 & )
	sleep 2
	$DIG $DIGOPTS soa . @10.53.0.2 > dig.out.ns2.test$n || ret=1
	grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
else
	echo "I:failed"
	status=1
fi

n=`expr $n + 1`
echo "I:checking that only the DLV key was imported from bind.keys ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 secroots 2>&1 | sed 's/^/I:ns2 /'
linecount=`grep "\./RSAMD5/.* ; managed" ns2/named.secroots | wc -l`
[ "$linecount" -eq 0 ] || ret=1
linecount=`grep "dlv.isc.org/RSAMD5/.* ; managed" ns2/named.secroots | wc -l`
[ "$linecount" -eq 2 ] || ret=1
linecount=`cat ns2/named.secroots | wc -l`
[ "$linecount" -eq 13 ] || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

exit $status
