#!/bin/sh
#
# Copyright (C) 2004, 2007, 2011, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2001  Internet Software Consortium.
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

# Id: tests.sh,v 1.11 2012/02/22 14:22:54 marka Exp 


# WARNING: The test labelled "testing request-ixfr option in view vs zone"
#          is fragile because it depends upon counting instances of records
#          in the log file - need a better approach <sdm> - until then,
#          if you add any tests above that point, you will break the test.

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"
DIGCMD="$DIG $DIGOPTS @10.53.0.1 -p 5300"
SENDCMD="$PERL ../send.pl 10.53.0.2 5301"
RNDCCMD="$RNDC -s 10.53.0.1 -p 9953 -c ../common/rndc.conf"

echo "I:testing initial AXFR"

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
/AXFR/
nil.      	300	NS	ns.nil.
nil.		300	TXT	"initial AXFR"
a.nil.		60	A	10.0.0.61
b.nil.		60	A	10.0.0.62
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
EOF

sleep 1

# Initially, ns1 is not authoritative for anything (see setup.sh).
# Now that ans is up and running with the right data, we make it
# a slave for nil.

cat <<EOF >>ns1/named.conf
zone "nil" {
	type slave;
	file "myftp.db";
	masters { 10.53.0.2; };
};
EOF

$RNDCCMD reload

for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIGCMD nil. SOA > dig.out
	grep "SOA" dig.out > /dev/null && break
	sleep 1
done

$DIGCMD nil. TXT | grep 'initial AXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:testing successful IXFR"

# We change the IP address of a.nil., and the TXT record at the apex.
# Then we do a SOA-only update.

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
/IXFR/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
a.nil.      	60	A	10.0.0.61
nil.		300	TXT	"initial AXFR"
nil.      	300	SOA	ns.nil. root.nil. 2 300 300 604800 300
nil.		300	TXT	"successful IXFR"
a.nil.      	60	A	10.0.1.61
nil.      	300	SOA	ns.nil. root.nil. 2 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
EOF

sleep 1

$RNDCCMD refresh nil

sleep 2

$DIGCMD nil. TXT | grep 'successful IXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:testing AXFR fallback after IXFR failure"

# Provide a broken IXFR response and a working fallback AXFR response

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
/IXFR/
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	TXT	"delete-nonexistent-txt-record"
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
nil.      	300	TXT	"this-txt-record-would-be-added"
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
/AXFR/
nil.      	300	NS	ns.nil.
nil.      	300	TXT	"fallback AXFR"
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
EOF

sleep 1

$RNDCCMD refresh nil

sleep 2

$DIGCMD nil. TXT | grep 'fallback AXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:testing ixfr-from-differences option"
# ns3 is master; ns4 is slave 
$CHECKZONE test. ns3/mytest.db > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "I:named-checkzone returned failure on ns3/mytest.db"
fi
# modify the master
#echo "I: digging against master: "
#$DIG $DIGOPTS @10.53.0.3 -p 5300 a host1.test.
#echo "I: digging against slave: "
#$DIG $DIGOPTS @10.53.0.4 -p 5300 a host1.test.

cp ns3/mytest1.db ns3/mytest.db
$RNDC -s 10.53.0.3 -p 9953 -c ../common/rndc.conf reload

for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG +tcp -p 5300 @10.53.0.4 SOA test > dig.out
	grep -i "hostmaster\.test\..2" dig.out > /dev/null && break
	sleep 1
done

# slave should have gotten notify and updated

for i in 0 1 2 3 4 5 6 7 8 9
do
	INCR=`grep "test/IN/primary" ns4/named.run|grep "got incremental"|wc -l`
	[ $INCR -eq 1 ] && break
	sleep 1
done
if [ $INCR -ne 1 ]
then
    echo "I:failed to get incremental response"
    status=1
fi

echo "I:testing request-ixfr option in view vs zone"
# There's a view with 2 zones. In the view, "request-ixfr yes"
# but in the zone "sub.test", request-ixfr no"
# we want to make sure that a change to sub.test results in AXFR, while
# changes to test. result in IXFR

echo "I: this result should be AXFR"
cp ns3/subtest1.db ns3/subtest.db # change to sub.test zone, should be AXFR
$RNDC -s 10.53.0.3 -p 9953 -c ../common/rndc.conf reload

for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG +tcp -p 5300 @10.53.0.4 SOA sub.test > dig.out
	grep -i "hostmaster\.test\..3" dig.out > /dev/null && break
	sleep 1
done

echo "I: this result should be AXFR"
for i in 0 1 2 3 4 5 6 7 8 9
do
	NONINCR=`grep 'sub\.test/IN/primary' ns4/named.run|grep "got nonincremental" | wc -l`
	[ $NONINCR -eq 2 ] && break
	sleep 1
done
if [ $NONINCR -ne 2 ]
then
    echo "I:failed to get nonincremental response in 2nd AXFR test"
    status=1
else
    echo "I:  success: AXFR it was"
fi

echo "I: this result should be IXFR"
cp ns3/mytest2.db ns3/mytest.db # change to test zone, should be IXFR
$RNDC -s 10.53.0.3 -p 9953 -c ../common/rndc.conf reload

for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG +tcp -p 5300 @10.53.0.4 SOA test > dig.out
	grep -i "hostmaster\.test\..4" dig.out > /dev/null && break
	sleep 1
done

for i in 0 1 2 3 4 5 6 7 8 9
do
	INCR=`grep "test/IN/primary" ns4/named.run|grep "got incremental"|wc -l`
	[ $INCR -eq 2 ] && break
	sleep 1
done
if [ $INCR -ne 2 ]
then
    echo "I:failed to get incremental response in 2nd IXFR test"
    status=1
else
    echo "I:  success: IXFR it was"
fi

echo "I:testing DiG's handling of a multi message AXFR style IXFR response" 
(
(sleep 10 && kill $$) 2>/dev/null &
sub=$!
$DIG ixfr=0 large -p 5300 @10.53.0.3 > dig.out
kill $sub
)
lines=`grep hostmaster.large dig.out | wc -l`
test ${lines:-0} -eq 2 || { echo "I:failed"; status=1; }
messages=`sed -n 's/^;;.*messages \([0-9]*\),.*/\1/p' dig.out`
test ${messages:-0} -gt 1 || { echo "I:failed"; status=1; }

echo "I:test 'dig +notcp ixfr=<value>' vs 'dig ixfr=<value> +notcp' vs 'dig ixfr=<value>'"
ret=0
# Should be "switch to TCP" response
$DIG +notcp ixfr=1 test -p 5300 @10.53.0.4 > dig.out1 || ret=1
$DIG ixfr=1 +notcp test -p 5300 @10.53.0.4 > dig.out2 || ret=1
$PERL ../digcomp.pl dig.out1 dig.out2 || ret=1
awk '$4 == "SOA" { soacnt++} END {if (soacnt == 1) exit(0); else exit(1);}' dig.out1 || ret=1
awk '$4 == "SOA" { if ($7 == 4) exit(0); else exit(1);}' dig.out1 || ret=1
# Should be incremental transfer.
$DIG ixfr=1 test -p 5300 @10.53.0.4 > dig.out3 || ret=1
awk '$4 == "SOA" { soacnt++} END { if (soacnt == 6) exit(0); else exit(1);}' dig.out3 || ret=1
if [ ${ret} != 0 ]; then
	echo "I:failed";
	status=1;
fi

echo "I:exit status: $status"
exit $status
