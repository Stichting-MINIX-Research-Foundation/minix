#!/bin/sh
#
# Copyright (C) 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.18 2012/02/23 06:53:15 marka Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +dnssec"

status=0
n=0

$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -nsec3param 1 0 0 - nsec3 > /dev/null 2>&1

for i in 1 2 3 4 5 6 7 8 9 0
do
	nsec3param=`$DIG +short @10.53.0.3 -p 5300 nsec3param nsec3.`
	test "$nsec3param" = "1 0 0 -" && break
	sleep 1
done

# Loop until retransfer3 has been transferred.
for i in 1 2 3 4 5 6 7 8 9 0
do
        ans=0
        $RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -nsec3param 1 0 0 - retransfer3 > /dev/null 2>&1 || ans=1
	[ $ans = 0 ] && break
done

for i in 1 2 3 4 5 6 7 8 9 0
do
	nsec3param=`$DIG +short @10.53.0.3 -p 5300 nsec3param retransfer3.`
	test "$nsec3param" = "1 0 0 -" && break
	sleep 1
done

n=`expr $n + 1`
echo "I:checking that rrsigs are replaced with ksk only"
ret=0
$DIG @10.53.0.3 -p 5300 axfr nsec3. |
	awk '/RRSIG NSEC3/ {a[$1]++} END { for (i in a) {if (a[i] != 1) exit (1)}}' || ret=1
#$DIG @10.53.0.3 -p 5300 axfr nsec3. | grep -w NSEC | grep -v "IN.RRSIG.NSEC" 
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that the zone is signed on initial transfer ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list bits > signing.out.test$n 2>&1
	keys=`grep '^Done signing' signing.out.test$n | wc -l`
	[ $keys = 2 ] || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking expired signatures are updated on load ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.3 -p 5300 +noall +answer +dnssec expired SOA > dig.out.ns3.test$n
expiry=`awk '$4 == "RRSIG" { print $9 }' dig.out.ns3.test$n`
[ "$expiry" = "20110101000000" ] && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking removal of private type record via 'rndc signing -clear' ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list bits > signing.out.test$n 2>&1
keys=`sed -n -e 's/Done signing with key \(.*\)$/\1/p' signing.out.test$n`
for key in $keys; do
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -clear ${key} bits > /dev/null || ret=1
	break;	# We only want to remove 1 record for now.
done 2>&1 |sed 's/^/I:ns3 /'

for i in 1 2 3 4 5 6 7 8 9 10
do
	ans=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list bits > signing.out.test$n 2>&1
        num=`grep "Done signing with" signing.out.test$n | wc -l`
	[ $num = 1 ] && break
	sleep 1
done
[ $ans = 0 ] || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking private type was properly signed ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.6 -p 5300 bits TYPE65534 > dig.out.ns6.test$n
grep "ANSWER: 2," dig.out.ns6.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns6.test$n > /dev/null || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking removal of remaining private type record via 'rndc signing -clear all' ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -clear all bits > /dev/null || ret=1

for i in 1 2 3 4 5 6 7 8 9 10
do
	ans=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list bits > signing.out.test$n 2>&1
	grep "No signing records found" signing.out.test$n > /dev/null || ans=1
	[ $ans = 1 ] || break
	sleep 1
done
[ $ans = 0 ] || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking negative private type response was properly signed ($n)"
ret=0
sleep 1
$DIG $DIGOPTS @10.53.0.6 -p 5300 bits TYPE65534 > dig.out.ns6.test$n
grep "status: NOERROR" dig.out.ns6.test$n > /dev/null || ret=1
grep "ANSWER: 0," dig.out.ns6.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns6.test$n > /dev/null || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone bits
server 10.53.0.2 5300
update add added.bits 0 A 1.2.3.4
send
EOF

n=`expr $n + 1`
echo "I:checking that the record is added on the hidden master ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 -p 5300 added.bits A > dig.out.ns2.test$n
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that update has been transfered and has been signed ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 added.bits A > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone bits
server 10.53.0.2 5300
update add bits 0 SOA ns2.bits. . 2011072400 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072400) serial on hidden master ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 -p 5300 bits SOA > dig.out.ns2.test$n
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns2.test$n > /dev/null || ret=1
grep "2011072400" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072400) serial in signed zone ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 bits SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072400" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I:checking that the zone is signed on initial transfer, noixfr ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 1 2 3 4 5 6 7 8 9 10 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list noixfr > signing.out.test$n 2>&1
	keys=`grep '^Done signing' signing.out.test$n | wc -l`
	[ $keys = 2 ] || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone noixfr
server 10.53.0.4 5300
update add added.noixfr 0 A 1.2.3.4
send
EOF

n=`expr $n + 1`
echo "I:checking that the record is added on the hidden master, noixfr ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.4 -p 5300 added.noixfr A > dig.out.ns4.test$n
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that update has been transfered and has been signed, noixfr ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 1 2 3 4 5 6 7 8 9 10 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 added.noixfr A > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone noixfr
server 10.53.0.4 5300
update add noixfr 0 SOA ns4.noixfr. . 2011072400 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072400) serial on hidden master, noixfr ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.4 -p 5300 noixfr SOA > dig.out.ns4.test$n
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns4.test$n > /dev/null || ret=1
grep "2011072400" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072400) serial in signed zone, noixfr ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 noixfr SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072400" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that the master zone signed on initial load ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list master  > signing.out.test$n 2>&1
	keys=`grep '^Done signing' signing.out.test$n | wc -l`
	[ $keys = 2 ] || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi

n=`expr $n + 1`
echo "I:checking removal of private type record via 'rndc signing -clear' (master) ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list master > signing.out.test$n 2>&1
keys=`sed -n -e 's/Done signing with key \(.*\)$/\1/p' signing.out.test$n`
for key in $keys; do
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -clear ${key} master > /dev/null || ret=1
	break;	# We only want to remove 1 record for now.
done 2>&1 |sed 's/^/I:ns3 /'

for i in 1 2 3 4 5 6 7 8 9
do
	ans=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list master > signing.out.test$n 2>&1
        num=`grep "Done signing with" signing.out.test$n | wc -l`
	[ $num = 1 ] && break
	sleep 1
done
[ $ans = 0 ] || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking private type was properly signed (master) ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.6 -p 5300 master TYPE65534 > dig.out.ns6.test$n
grep "ANSWER: 2," dig.out.ns6.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns6.test$n > /dev/null || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking removal of remaining private type record via 'rndc signing -clear' (master) ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -clear all master > /dev/null || ret=1
for i in 1 2 3 4 5 6 7 8 9 10
do
	ans=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list master > signing.out.test$n 2>&1
	grep "No signing records found" signing.out.test$n > /dev/null || ans=1
	[ $ans = 1 ] || break
	sleep 1
done
[ $ans = 0 ] || ret=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check adding of record to unsigned master ($n)"
ret=0
cp ns3/master2.db.in ns3/master.db
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload master || ret=1
for i in 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 e.master A > dig.out.ns3.test$n
	grep "10.0.0.5" dig.out.ns3.test$n > /dev/null || ans=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 1 ] || break
	sleep 1
done
[ $ans = 0 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check adding record fails when SOA serial not changed ($n)"
ret=0
echo "c A 10.0.0.3" >> ns3/master.db
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload || ret=1
sleep 1
$DIG $DIGOPTS @10.53.0.3 -p 5300 c.master A > dig.out.ns3.test$n
grep "NXDOMAIN" dig.out.ns3.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check adding record works after updating SOA serial ($n)"
ret=0
cp ns3/master3.db.in ns3/master.db
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload master || ret=1
for i in 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 c.master A > dig.out.ns3.test$n
	grep "10.0.0.3" dig.out.ns3.test$n > /dev/null || ans=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 1 ] || break
	sleep 1
done
[ $ans = 0 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check the added record was properly signed ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.3 -p 5300 e.master A > dig.out.ns6.test$n
grep "10.0.0.5" dig.out.ns6.test$n > /dev/null || ans=1
grep "ANSWER: 2," dig.out.ns6.test$n > /dev/null || ans=1
grep "flags:.* ad[ ;]" dig.out.ns6.test$n > /dev/null || ans=1

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that the dynamic master zone signed on initial load ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 signing -list dynamic > signing.out.test$n 2>&1
	keys=`grep '^Done signing' signing.out.test$n | wc -l`
	[ $keys = 2 ] || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi

n=`expr $n + 1`
echo "I:checking master zone that was updated while offline is correct ($n)"
ret=0
serial=`$DIG $DIGOPTS +short @10.53.0.3 -p 5300 updated SOA | awk '{print $3}'`
# serial should have changed
[ "$serial" = "2000042407" ] && ret=1
# e.updated should exist and should be signed
$DIG $DIGOPTS @10.53.0.3 -p 5300 e.updated A > dig.out.ns3.test$n
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
# updated.db.signed.jnl should exist, should have the source serial
# of master2.db, and should show a minimal diff: no more than 8 added
# records (SOA/RRSIG, 2 x NSEC/RRSIG, A/RRSIG), and 4 removed records
# (SOA/RRSIG, NSEC/RRSIG).
serial=`$JOURNALPRINT ns3/updated.db.signed.jnl | head -1 | awk '{print $4}'`
[ "$serial" = "2000042408" ] || ret=1
diffsize=`$JOURNALPRINT ns3/updated.db.signed.jnl | wc -l`
[ "$diffsize" -le 13 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking adding of record to unsigned master using UPDATE ($n)"
ret=0

[ -f ns3/dynamic.db.jnl ] && { ret=1 ; echo "I:journal exists (pretest)" ; }

$NSUPDATE << EOF
zone dynamic
server 10.53.0.3 5300
update add e.dynamic 0 A 1.2.3.4
send
EOF

[ -f ns3/dynamic.db.jnl ] || { ret=1 ; echo "I:journal does not exist (posttest)" ; }

for i in 1 2 3 4 5 6 7 8 9 10
do 
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 e.dynamic > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ans=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ans=1
	grep "1.2.3.4" dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 0 ] && break
	sleep 1
done
[ $ans = 0 ] || { ret=1; echo "I:signed record not found"; cat dig.out.ns3.test$n ; }

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:stop bump in the wire signer server ($n)"
ret=0
$PERL ../stop.pl . ns3 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:restart bump in the wire signer server ($n)"
ret=0
$PERL ../start.pl --noclean --restart . ns3 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone bits
server 10.53.0.2 5300
update add bits 0 SOA ns2.bits. . 2011072450 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072450) serial on hidden master ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 -p 5300 bits SOA > dig.out.ns2.test$n
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns2.test$n > /dev/null || ret=1
grep "2011072450" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072450) serial in signed zone ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 bits SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072450" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone noixfr
server 10.53.0.4 5300
update add noixfr 0 SOA ns4.noixfr. . 2011072450 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072450) serial on hidden master, noixfr ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.4 -p 5300 noixfr SOA > dig.out.ns4.test$n
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns4.test$n > /dev/null || ret=1
grep "2011072450" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking YYYYMMDDVV (2011072450) serial in signed zone, noixfr ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 noixfr SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072450" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone bits
server 10.53.0.3 5300
update add bits 0 SOA ns2.bits. . 2011072460 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking forwarded update on hidden master ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 -p 5300 bits SOA > dig.out.ns2.test$n
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns2.test$n > /dev/null || ret=1
grep "2011072460" dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking forwarded update on signed zone ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 bits SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072460" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone noixfr
server 10.53.0.3 5300
update add noixfr 0 SOA ns4.noixfr. . 2011072460 20 20 1814400 3600
send
EOF

n=`expr $n + 1`
echo "I:checking forwarded update on hidden master, noixfr ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.4 -p 5300 noixfr SOA > dig.out.ns4.test$n
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns4.test$n > /dev/null || ret=1
grep "2011072460" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking forwarded update on signed zone, noixfr ($n)"
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 noixfr SOA > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
	grep "2011072460" dig.out.ns3.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking turning on of inline signing in a slave zone via reload ($n)"
$DIG $DIGOPTS @10.53.0.5 -p 5300 +dnssec bits SOA > dig.out.ns5.test$n
grep "status: NOERROR" dig.out.ns5.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns5.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:setup broken"; fi
status=`expr $status + $ret`
cp ns5/named.conf.post ns5/named.conf
(cd ns5; $KEYGEN -q -r $RANDFILE bits) > /dev/null 2>&1
(cd ns5; $KEYGEN -q -r $RANDFILE -f KSK bits) > /dev/null 2>&1
$RNDC -c ../common/rndc.conf -s 10.53.0.5 -p 9953 reload 2>&1 | sed 's/^/I:ns5 /'
for i in 1 2 3 4 5 6 7 8 9 10
do
	ret=0
	$DIG $DIGOPTS @10.53.0.5 -p 5300 bits SOA > dig.out.ns5.test$n
	grep "status: NOERROR" dig.out.ns5.test$n > /dev/null || ret=1
	grep "ANSWER: 2," dig.out.ns5.test$n > /dev/null || ret=1
	if [ $ret = 0 ]; then break; fi
	sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking rndc freeze/thaw of dynamic inline zone no change ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 freeze dynamic > freeze.test$n 2>&1 || { echo "I: rndc freeze dynamic failed" ; sed 's/^/I:/' < freeze.test$n ; ret=1;  }
sleep 1
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 thaw dynamic > thaw.test$n 2>&1 || { echo "I: rndc thaw dynamic failed" ; ret=1; }
sleep 1
grep "zone dynamic/IN (unsigned): ixfr-from-differences: unchanged" ns3/named.run > /dev/null ||  ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`


n=`expr $n + 1`
echo "I:checking rndc freeze/thaw of dynamic inline zone ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 freeze dynamic > freeze.test$n 2>&1 || ret=1 
sleep 1
awk '$2 == ";" && $3 == "serial" { printf("%d %s %s\n", $1 + 1, $2, $3); next; }
     { print; }
     END { print "freeze1.dynamic. 0 TXT freeze1"; } ' ns3/dynamic.db > ns3/dynamic.db.new
mv ns3/dynamic.db.new ns3/dynamic.db
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 thaw dynamic > thaw.test$n 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check added record freeze1.dynamic ($n)"
for i in 1 2 3 4 5 6 7 8 9
do
    ret=0
    $DIG $DIGOPTS @10.53.0.3 -p 5300 freeze1.dynamic TXT > dig.out.ns3.test$n
    grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
    grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
    test $ret = 0 && break
    sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# allow 1 second so that file time stamps change
sleep 1

n=`expr $n + 1`
echo "I:checking rndc freeze/thaw of server ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 freeze > freeze.test$n 2>&1 || ret=1
sleep 1
awk '$2 == ";" && $3 == "serial" { printf("%d %s %s\n", $1 + 1, $2, $3); next; }
     { print; }
     END { print "freeze2.dynamic. 0 TXT freeze2"; } ' ns3/dynamic.db > ns3/dynamic.db.new
mv ns3/dynamic.db.new ns3/dynamic.db
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 thaw > thaw.test$n 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check added record freeze2.dynamic ($n)"
for i in 1 2 3 4 5 6 7 8 9
do
    ret=0
    $DIG $DIGOPTS @10.53.0.3 -p 5300 freeze2.dynamic TXT > dig.out.ns3.test$n
    grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
    grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ret=1
    test $ret = 0 && break
    sleep 1
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check rndc reload allows reuse of inline-signing zones ($n)"
ret=0
{ $RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 || ret=1 ; } |
sed 's/^/I:ns3 /'
grep "not reusable" ns3/named.run > /dev/null 2>&1 && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:check rndc sync removes both signed and unsigned journals ($n)"
ret=0
[ -f ns3/dynamic.db.jnl ] || ret=1
[ -f ns3/dynamic.db.signed.jnl ] || ret=1
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 sync -clean dynamic 2>&1 || ret=1
[ -f ns3/dynamic.db.jnl ] && ret=1
[ -f ns3/dynamic.db.signed.jnl ] && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

$NSUPDATE << EOF
zone retransfer
server 10.53.0.2 5300
update add added.retransfer 0 A 1.2.3.4
send

EOF

n=`expr $n + 1`
echo "I:checking that the retransfer record is added on the hidden master ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.2 -p 5300 added.retransfer A > dig.out.ns2.test$n
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking that the change has not been transfered due to notify ($n)"
ret=0
for i in 0 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 added.retransfer A > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 0 ] && break
	sleep 1
done
if [ $ans != 1 ]; then echo "I:failed"; ret=1; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I:check rndc retransfer of a inline slave zone works ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 retransfer retransfer 2>&1 || ret=1
for i in 0 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 added.retransfer A > dig.out.ns3.test$n
	grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ans=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 0 ] && break
	sleep 1
done
[ $ans = 1 ] && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check rndc retransfer of a inline nsec3 slave retains nsec3 ($n)"
ret=0
for i in 0 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 nonexist.retransfer3 A > dig.out.ns3.pre.test$n
	grep "status: NXDOMAIN" dig.out.ns3.pre.test$n > /dev/null || ans=1
	grep "NSEC3" dig.out.ns3.pre.test$n > /dev/null || ans=1
	[ $ans = 0 ] && break
	sleep 1
done
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 retransfer retransfer3 2>&1 || ret=1
for i in 0 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 nonexist.retransfer3 A > dig.out.ns3.post.test$n
	grep "status: NXDOMAIN" dig.out.ns3.post.test$n > /dev/null || ans=1
	grep "NSEC3" dig.out.ns3.post.test$n > /dev/null || ans=1
	[ $ans = 0 ] && break
	sleep 1
done
[ $ans = 1 ] && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:stop bump in the wire signer server ($n)"
ret=0
$PERL ../stop.pl . ns3 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:update SOA record while stopped"
cp ns3/master4.db.in ns3/master.db
rm ns3/master.db.jnl

n=`expr $n + 1`
echo "I:restart bump in the wire signer server ($n)"
ret=0
$PERL ../start.pl --noclean --restart . ns3 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:updates to SOA parameters other than serial while stopped are reflected in signed zone ($n)"
ret=0
for i in 1 2 3 4 5 6 7 8 9
do
	ans=0
	$DIG $DIGOPTS @10.53.0.3 -p 5300 master SOA > dig.out.ns3.test$n
	grep "hostmaster" dig.out.ns3.test$n > /dev/null || ans=1
	grep "ANSWER: 2," dig.out.ns3.test$n > /dev/null || ans=1
	[ $ans = 1 ] || break
	sleep 1
done
[ $ans = 0 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:test add/del zone combinations ($n)"
ret=0
for zone in a b c d e f g h i j k l m n o p q r s t u v w x y z
do
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 addzone test-$zone \
	'{ type master; file "bits.db.in"; allow-transfer { any; }; };'
$DIG $DIGOPTS @10.53.0.2 -p 5300 test-$zone SOA > dig.out.ns2.$zone.test$n
grep "status: NOERROR," dig.out.ns2.$zone.test$n  > /dev/null || { ret=1; cat dig.out.ns2.$zone.test$n; }
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 addzone test-$zone \
	'{ type slave; masters { 10.53.0.2; }; file "'test-$zone.bk'"; inline-signing yes; auto-dnssec maintain; allow-transfer { any; }; };'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 delzone test-$zone > /dev/null 2>&1
done

n=`expr $n + 1`
echo "I:testing adding external keys to a inline zone ($n)"
ret=0
$DIG $DIGOPTS @10.53.0.3 -p 5300 dnskey externalkey > dig.out.ns3.test$n
for alg in 3 7 12 13
do
   [ $alg = 3 -a ! -f checkdsa ] && continue;
   [ $alg = 12 -a ! -f checkgost ] && continue;
   [ $alg = 13 -a ! -f checkecdsa ] && continue;

   case $alg in
   3) echo "I: checking DSA";;
   7) echo "I: checking NSEC3RSASHA1";;
   12) echo "I: checking GOST";;
   13) echo "I: checking ECDSAP256SHA256";;
   *) echo "I: checking $alg";;
   esac

   dnskeys=`grep "IN.DNSKEY.25[67] [0-9]* $alg " dig.out.ns3.test$n | wc -l`
   rrsigs=`grep "RRSIG.DNSKEY $alg " dig.out.ns3.test$n | wc -l`
   test ${dnskeys:-0} -eq 3 || { echo "I: failed $alg (dnskeys ${dnskeys:-0})"; ret=1; }
   test ${rrsigs:-0} -eq 2 || { echo "I: failed $alg (rrsigs ${rrsigs:-0})"; ret=1; }
done
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:testing imported key won't overwrite a private key ($n)"
ret=0
key=`$KEYGEN -r $RANDFILE -q import.example`
cp ${key}.key import.key
# import should fail
$IMPORTKEY -f import.key import.example > /dev/null 2>&1 && ret=1
rm -f ${key}.private
# private key removed; import should now succeed
$IMPORTKEY -f import.key import.example > /dev/null 2>&1 || ret=1
# now that it's an external key, re-import should succeed
$IMPORTKEY -f import.key import.example > /dev/null 2>&1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

exit $status
