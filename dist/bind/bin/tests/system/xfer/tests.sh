#!/bin/sh
#
# Copyright (C) 2004, 2005, 2007, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.31.814.4 2011-03-11 00:47:27 marka Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"

status=0

echo "I:testing basic zone transfer functionality"
$DIG $DIGOPTS example. \
	@10.53.0.2 axfr -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

#
# Spin to allow the zone to tranfer.
#
for i in 1 2 3 4 5
do
tmp=0
$DIG $DIGOPTS example. \
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || tmp=1
	grep ";" dig.out.ns3 > /dev/null
	if test $? -ne 0 ; then break; fi
	echo "I: plain zone re-transfer"
	sleep 5
done
if test $tmp -eq 1 ; then status=1; fi
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig1.good dig.out.ns2 || status=1

$PERL ../digcomp.pl dig1.good dig.out.ns3 || status=1

echo "I:testing TSIG signed zone transfers"
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.2 axfr -y tsigzone.:1234abcd8765 -p 5300 \
	> dig.out.ns2 || status=1
grep ";" dig.out.ns2

#
# Spin to allow the zone to tranfer.
#
for i in 1 2 3 4 5
do
tmp=0
$DIG $DIGOPTS tsigzone. \
    	@10.53.0.3 axfr -y tsigzone.:1234abcd8765 -p 5300 \
	> dig.out.ns3 || tmp=1
	grep ";" dig.out.ns3 > /dev/null
	if test $? -ne 0 ; then break; fi
	echo "I: plain zone re-transfer"
	sleep 5
done
if test $tmp -eq 1 ; then status=1; fi
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig.out.ns2 dig.out.ns3 || status=1

echo "I:reload servers for in preparation for ixfr-from-differences tests"

$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload 2>&1 | sed 's/^/I:ns1 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 | sed 's/^/I:ns3 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.6 -p 9953 reload 2>&1 | sed 's/^/I:ns6 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.7 -p 9953 reload 2>&1 | sed 's/^/I:ns7 /'

sleep 2

echo "I:updating master zones for ixfr-from-differences tests"

$PERL -i -p -e '
	s/0\.0\.0\.0/0.0.0.1/;
	s/1397051952/1397051953/
' ns1/slave.db

$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload 2>&1 | sed 's/^/I:ns1 /'

$PERL -i -p -e '
	s/0\.0\.0\.0/0.0.0.1/;
	s/1397051952/1397051953/
' ns2/example.db

$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'

$PERL -i -p -e '
	s/0\.0\.0\.0/0.0.0.1/;
	s/1397051952/1397051953/
' ns6/master.db

$RNDC -c ../common/rndc.conf -s 10.53.0.6 -p 9953 reload 2>&1 | sed 's/^/I:ns6 /'

$PERL -i -p -e '
	s/0\.0\.0\.0/0.0.0.1/;
	s/1397051952/1397051953/
' ns7/master2.db

$RNDC -c ../common/rndc.conf -s 10.53.0.7 -p 9953 reload 2>&1 | sed 's/^/I:ns7 /'

sleep 3

echo "I:testing ixfr-from-differences yes;"
tmp=0

$DIG $DIGOPTS example. \
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || tmp=1
grep ";" dig.out.ns3

$PERL ../digcomp.pl dig2.good dig.out.ns3 || tmp=1

# ns3 has a journal iff it received an IXFR.
test -f ns3/example.bk || tmp=1 
test -f ns3/example.bk.jnl || tmp=1 

if test $tmp != 0 ; then echo "I:failed"; fi
status=`expr $status + $tmp`

echo "I:testing ixfr-from-differences master; (master zone)"
tmp=0

$DIG $DIGOPTS master. \
	@10.53.0.6 axfr -p 5300 > dig.out.ns6 || tmp=1
grep ";" dig.out.ns6

$DIG $DIGOPTS master. \
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || tmp=1
grep ";" dig.out.ns3 && cat dig.out.ns3

$PERL ../digcomp.pl dig.out.ns6 dig.out.ns3 || tmp=1

# ns3 has a journal iff it received an IXFR.
test -f ns3/master.bk || tmp=1 
test -f ns3/master.bk.jnl || tmp=1 

if test $tmp != 0 ; then echo "I:failed"; fi
status=`expr $status + $tmp`

echo "I:testing ixfr-from-differences master; (slave zone)"
tmp=0

$DIG $DIGOPTS slave. \
	@10.53.0.6 axfr -p 5300 > dig.out.ns6 || tmp=1
grep ";" dig.out.ns6

$DIG $DIGOPTS slave. \
	@10.53.0.1 axfr -p 5300 > dig.out.ns1 || tmp=1
grep ";" dig.out.ns1

$PERL ../digcomp.pl dig.out.ns6 dig.out.ns1 || tmp=1

# ns6 has a journal iff it received an IXFR.
test -f ns6/slave.bk || tmp=1 
test -f ns6/slave.bk.jnl && tmp=1 

if test $tmp != 0 ; then echo "I:failed"; fi
status=`expr $status + $tmp`

echo "I:testing ixfr-from-differences slave; (master zone)"
tmp=0

# ns7 has a journal iff it generates an IXFR.
test -f ns7/master2.db || tmp=1 
test -f ns7/master2.db.jnl && tmp=1 

if test $tmp != 0 ; then echo "I:failed"; fi
status=`expr $status + $tmp`
echo "I:testing ixfr-from-differences slave; (slave zone)"
tmp=0

$DIG $DIGOPTS slave. \
	@10.53.0.1 axfr -p 5300 > dig.out.ns1 || tmp=1
grep ";" dig.out.ns1

$DIG $DIGOPTS slave. \
	@10.53.0.7 axfr -p 5300 > dig.out.ns7 || tmp=1
grep ";" dig.out.ns1

$PERL ../digcomp.pl dig.out.ns7 dig.out.ns1 || tmp=1

# ns7 has a journal iff it generates an IXFR.
test -f ns7/slave.bk || tmp=1 
test -f ns7/slave.bk.jnl || tmp=1 

if test $tmp != 0 ; then echo "I:failed"; fi
status=`expr $status + $tmp`

# now we test transfers with assorted TSIG glitches
DIGCMD="$DIG $DIGOPTS @10.53.0.4 -p 5300"
SENDCMD="$PERL ../send.pl 10.53.0.5 5301"
RNDCCMD="$RNDC -s 10.53.0.4 -p 9953 -c ../common/rndc.conf"

echo "I:testing that incorrectly signed transfers will fail..."
echo "I:initial correctly-signed transfer should succeed"

$SENDCMD < ans5/goodaxfr
sleep 1

# Initially, ns4 is not authoritative for anything.
# Now that ans is up and running with the right data, we make it
# a slave for nil.

cat <<EOF >>ns4/named.conf
zone "nil" {
	type slave;
	file "nil.db";
	masters { 10.53.0.5 key tsig_key; };
};
EOF

$RNDCCMD reload | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'initial AXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:unsigned transfer"

$SENDCMD < ans5/unsigned
sleep 1

$RNDCCMD retransfer nil | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'unsigned AXFR' >/dev/null && {
    echo "I:failed"
    status=1
}

echo "I:bad keydata"

$SENDCMD < ans5/badkeydata
sleep 1

$RNDCCMD retransfer nil | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'bad keydata AXFR' >/dev/null && {
    echo "I:failed"
    status=1
}

echo "I:partially-signed transfer"

$SENDCMD < ans5/partial
sleep 1

$RNDCCMD retransfer nil | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'partially signed AXFR' >/dev/null && {
    echo "I:failed"
    status=1
}

echo "I:unknown key"

$SENDCMD < ans5/unknownkey
sleep 1

$RNDCCMD retransfer nil | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'unknown key AXFR' >/dev/null && {
    echo "I:failed"
    status=1
}

echo "I:incorrect key"

$SENDCMD < ans5/wrongkey
sleep 1

$RNDCCMD retransfer nil | sed 's/^/I:ns4 /'

sleep 2

$DIGCMD nil. TXT | grep 'incorrect key AXFR' >/dev/null && {
    echo "I:failed"
    status=1
}

echo "I:exit status: $status"
exit $status
