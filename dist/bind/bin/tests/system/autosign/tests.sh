#!/bin/sh
#
# Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.12.18.16 2011-07-26 04:41:48 marka Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=random.data

status=0
n=0

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd +dnssec -p 5300"

# convert private-type records to readable form
showprivate () {
    echo "-- $@ --"
    $DIG $DIGOPTS +nodnssec +short @$2 -t type65534 $1 | cut -f3 -d' ' |
        while read record; do
            perl -e 'my $rdata = pack("H*", @ARGV[0]);
                die "invalid record" unless length($rdata) == 5;
                my ($alg, $key, $remove, $complete) = unpack("CnCC", $rdata);
                my $action = "signing";
                $action = "removing" if $remove;
                my $state = " (incomplete)";
                $state = " (complete)" if $complete;
                print ("$action: alg: $alg, key: $key$state\n");' $record
        done
}

# check that signing records are marked as complete
checkprivate () {
    ret=0
    x=`showprivate "$@"`
    echo $x | grep incomplete >&- 2>&- && ret=1
    [ $ret = 1 ] && {
        echo "$x"
        echo "I:failed"
    }
    return $ret
}

#
#  The NSEC record at the apex of the zone and its RRSIG records are
#  added as part of the last step in signing a zone.  We wait for the
#  NSEC records to appear before proceeding with a counter to prevent
#  infinite loops if there is a error.
#
echo "I:waiting for autosign changes to take effect"
i=0
while [ $i -lt 30 ]
do
	ret=0
	#
	# Wait for the root DNSKEY RRset to be fully signed.
	#
	$DIG $DIGOPTS . @10.53.0.1 dnskey > dig.out.ns1.test$n || ret=1
	grep "ANSWER: 10," dig.out.ns1.test$n > /dev/null || ret=1
	for z in .
	do
		$DIG $DIGOPTS $z @10.53.0.1 nsec > dig.out.ns1.test$n || ret=1
		grep "NS SOA" dig.out.ns1.test$n > /dev/null || ret=1
	done
	for z in bar. example. private.secure.example.
	do
		$DIG $DIGOPTS $z @10.53.0.2 nsec > dig.out.ns2.test$n || ret=1
		grep "NS SOA" dig.out.ns2.test$n > /dev/null || ret=1
	done
	for z in bar. example.
	do 
		$DIG $DIGOPTS $z @10.53.0.3 nsec > dig.out.ns3.test$n || ret=1
		grep "NS SOA" dig.out.ns3.test$n > /dev/null || ret=1
	done
	i=`expr $i + 1`
	if [ $ret = 0 ]; then break; fi
	echo "I:waiting ... ($i)"
	sleep 2
done
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; else echo "I:done"; fi
status=`expr $status + $ret`

echo "I:checking NSEC->NSEC3 conversion prerequisites ($n)"
ret=0
# this command should result in an empty file:
$DIG $DIGOPTS +noall +answer nsec3.example. nsec3param @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "NSEC3PARAM" dig.out.ns3.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking NSEC3->NSEC conversion prerequisites ($n)"
ret=0
$DIG $DIGOPTS +noall +answer nsec3-to-nsec.example. nsec3param @10.53.0.3 > dig.out.ns3.test$n || ret=1
grep "NSEC3PARAM" dig.out.ns3.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:converting zones from nsec to nsec3"
$NSUPDATE > /dev/null 2>&1 <<END	|| status=1
server 10.53.0.3 5300
zone nsec3.nsec3.example.
update add nsec3.nsec3.example. 3600 NSEC3PARAM 1 0 10 BEEF
send
zone optout.nsec3.example.
update add optout.nsec3.example. 3600 NSEC3PARAM 1 1 10 BEEF
send
zone nsec3.example.
update add nsec3.example. 3600 NSEC3PARAM 1 0 10 BEEF
send
zone nsec3.optout.example.
update add nsec3.optout.example. 3600 NSEC3PARAM 1 0 10 BEEF
send
zone optout.optout.example.
update add optout.optout.example. 3600 NSEC3PARAM 1 1 10 BEEF
send
zone optout.example.
update add optout.example. 3600 NSEC3PARAM 1 1 10 BEEF
send
END

# try to convert nsec.example; this should fail due to non-NSEC key
$NSUPDATE > nsupdate.out 2>&1 <<END
server 10.53.0.3 5300
zone nsec.example.
update add nsec.example. 3600 NSEC3PARAM 1 0 10 BEEF
send
END

echo "I:waiting for changes to take effect"
sleep 3

echo "I:converting zone from nsec3 to nsec"
$NSUPDATE > /dev/null 2>&1 << END	|| status=1
server 10.53.0.3 5300
zone nsec3-to-nsec.example.
update delete nsec3-to-nsec.example. NSEC3PARAM
send
END

echo "I:waiting for change to take effect"
sleep 3

echo "I:checking that expired RRSIGs from missing key are not deleted ($n)"
ret=0
missing=`sed 's/^K.*+007+0*\([0-9]\)/\1/' < missingzsk.key`
$JOURNALPRINT ns3/nozsk.example.db.jnl | \
   awk '{if ($1 == "del" && $5 == "RRSIG" && $12 == id) {exit 1}} END {exit 0}' id=$missing || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that expired RRSIGs from inactive key are not deleted ($n)"
ret=0
inactive=`sed 's/^K.*+007+0*\([0-9]\)/\1/' < inactivezsk.key`
$JOURNALPRINT ns3/inaczsk.example.db.jnl | \
   awk '{if ($1 == "del" && $5 == "RRSIG" && $12 == id) {exit 1}} END {exit 0}' id=$inactive || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that non-replaceable RRSIGs are logged only once ($n)"
ret=0
loglines=`grep "Key nozsk.example/NSEC3RSASHA1/$missing .* retaining signatures" ns3/named.run | wc -l`
[ "$loglines" -eq 1 ] || ret=1
loglines=`grep "Key inaczsk.example/NSEC3RSASHA1/$missing .* retaining signatures" ns3/named.run | wc -l`
[ "$loglines" -eq 1 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# This test is above the rndc freeze/thaw calls because the apex node
# will be resigned on thaw, increasing the serial number again.
echo "I:checking serial is not incremented when signatures are unchanged ($n)"
ret=0
newserial=`$DIG $DIGOPTS +short soa nozsk.example @10.53.0.3 | awk '$0 !~ /SOA/ {print $3}'`
[ "$newserial" -eq 2 ] || ret=1
newserial=`$DIG $DIGOPTS +short soa inaczsk.example @10.53.0.3 | awk '$0 !~ /SOA/ {print $3}'`
[ "$newserial" -eq 2 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Send rndc freeze command to ns1, ns2 and ns3, to force the dynamically
# signed zones to be dumped to their zone files
echo "I:dumping zone files"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 freeze 2>&1 | sed 's/^/I:ns1 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 thaw 2>&1 | sed 's/^/I:ns1 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 freeze 2>&1 | sed 's/^/I:ns2 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 thaw 2>&1 | sed 's/^/I:ns2 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 freeze 2>&1 | sed 's/^/I:ns3 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 thaw 2>&1 | sed 's/^/I:ns3 /'

echo "I:checking expired signatures were updated ($n)"
ret=0
$DIG $DIGOPTS +noauth a.oldsigs.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.oldsigs.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking NSEC->NSEC3 conversion succeeded ($n)"
ret=0
$DIG $DIGOPTS nsec3.example. nsec3param @10.53.0.3 > dig.out.ns3.ok.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.ok.test$n > /dev/null || ret=1
$DIG $DIGOPTS +noauth q.nsec3.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth q.nsec3.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking NSEC->NSEC3 conversion failed with NSEC-only key ($n)"
ret=0
grep "failed: REFUSED" nsupdate.out > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking NSEC3->NSEC conversion succeeded ($n)"
ret=0
# this command should result in an empty file:
$DIG $DIGOPTS +noall +answer nsec3-to-nsec.example. nsec3param @10.53.0.3 > dig.out.ns3.nx.test$n || ret=1
grep "NSEC3PARAM" dig.out.ns3.nx.test$n > /dev/null && ret=1
$DIG $DIGOPTS +noauth q.nsec3-to-nsec.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth q.nsec3-to-nsec.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive validation NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth a.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive validation NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive validation OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NXDOMAIN NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth q.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth q.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NXDOMAIN NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth q.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth q.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NXDOMAIN OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth q.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth q.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NODATA NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.example. @10.53.0.2 txt > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth a.example. @10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NODATA NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.3 txt > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative validation NODATA OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.3 txt > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 0" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check the insecure.example domain

echo "I:checking 1-server insecurity proof NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.insecure.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.insecure.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server negative insecurity proof NSEC ($n)"
ret=0
$DIG $DIGOPTS q.insecure.example. a @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS q.insecure.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check the secure.example domain

echo "I:checking multi-stage positive validation NSEC/NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.secure.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.secure.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation NSEC/NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation NSEC/OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation NSEC3/NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.secure.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.secure.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation NSEC3/NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nsec3.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.nsec3.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation NSEC3/OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.optout.nsec3.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.optout.nsec3.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation OPTOUT/NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.secure.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.secure.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation OPTOUT/NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nsec3.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.nsec3.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking multi-stage positive validation OPTOUT/OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.optout.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.optout.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking empty NODATA OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth empty.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth empty.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
#grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check the insecure.secure.example domain (insecurity proof)

echo "I:checking 2-server insecurity proof ($n)"
ret=0
$DIG $DIGOPTS +noauth a.insecure.secure.example. @10.53.0.2 a \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth a.insecure.secure.example. @10.53.0.4 a \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check a negative response in insecure.secure.example

echo "I:checking 2-server insecurity proof with a negative answer ($n)"
ret=0
$DIG $DIGOPTS q.insecure.secure.example. @10.53.0.2 a > dig.out.ns2.test$n \
	|| ret=1
$DIG $DIGOPTS q.insecure.secure.example. @10.53.0.4 a > dig.out.ns4.test$n \
	|| ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking security root query ($n)"
ret=0
$DIG $DIGOPTS . @10.53.0.4 key > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive validation RSASHA256 NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.rsasha256.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.rsasha256.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive validation RSASHA512 NSEC ($n)"
ret=0
$DIG $DIGOPTS +noauth a.rsasha512.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.rsasha512.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that positive validation in a privately secure zone works ($n)"
ret=0
$DIG $DIGOPTS +noauth a.private.secure.example. a @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth a.private.secure.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that negative validation in a privately secure zone works ($n)"
ret=0
$DIG $DIGOPTS +noauth q.private.secure.example. a @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth q.private.secure.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking privately secure to nxdomain works ($n)"
ret=0
$DIG $DIGOPTS +noauth private2secure-nxdomain.private.secure.example. SOA @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth private2secure-nxdomain.private.secure.example. SOA @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Try validating with a revoked trusted key.
# This should fail.

echo "I:checking that validation returns insecure due to revoked trusted key ($n)"
ret=0
$DIG $DIGOPTS example. soa @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "flags:.*; QUERY" dig.out.ns5.test$n > /dev/null || ret=1
grep "flags:.* ad.*; QUERY" dig.out.ns5.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that revoked key is present ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < rev.key`
id=`expr $id + 128 % 65536`
$DIG $DIGOPTS +multi dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep '; key id = '"$id"'$' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that revoked key self-signs ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < rev.key`
id=`expr $id + 128 % 65536`
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $id "'\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for unpublished key ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < unpub.key`
$DIG $DIGOPTS +multi dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep '; key id = '"$id"'$' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that standby key does not sign records ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < standby.key`
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $id "'\. ' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that deactivated key does not sign records  ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < inact.key`
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $id "'\. ' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking insertion of public-only key ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < nopriv.key`
file="ns1/`cat nopriv.key`.key"
keydata=`grep DNSKEY $file`
$NSUPDATE > /dev/null 2>&1 <<END	|| status=1
server 10.53.0.1 5300
zone .
ttl 3600
update add $keydata
send
END
sleep 1
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $id "'\. ' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking key deletion ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < del.key`
$DIG $DIGOPTS +multi dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep '; key id = '"$id"'$' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking secure-to-insecure transition, nsupdate ($n)"
$NSUPDATE > /dev/null 2>&1 <<END	|| status=1
server 10.53.0.3 5300
zone secure-to-insecure.example
update delete secure-to-insecure.example dnskey
send
END
sleep 2
$DIG $DIGOPTS axfr secure-to-insecure.example @10.53.0.3 > dig.out.ns3.test$n || ret=1
egrep 'RRSIG' dig.out.ns3.test$n > /dev/null && ret=1
egrep '(DNSKEY|NSEC)' dig.out.ns3.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking secure-to-insecure transition, scheduled ($n)"
file="ns3/`cat del1.key`.key"
$SETTIME -I now -D now $file > /dev/null
file="ns3/`cat del2.key`.key"
$SETTIME -I now -D now $file > /dev/null
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 sign secure-to-insecure2.example. 2>&1 | sed 's/^/I:ns3 /'
sleep 2
$DIG $DIGOPTS axfr secure-to-insecure2.example @10.53.0.3 > dig.out.ns3.test$n || ret=1
egrep 'RRSIG' dig.out.ns3.test$n > /dev/null && ret=1
egrep '(DNSKEY|NSEC3)' dig.out.ns3.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that serial number and RRSIGs are both updated (rt21045) ($n)"
ret=0
oldserial=`$DIG $DIGOPTS +short soa prepub.example @10.53.0.3 | awk '$0 !~ /SOA/ {print $3}'`
oldinception=`$DIG $DIGOPTS +short soa prepub.example @10.53.0.3 | awk '/SOA/ {print $6}' | sort -u`

$KEYGEN -3 -q -r $RANDFILE -K ns3 -P 0 -A +6d -I +38d -D +45d prepub.example > /dev/null

$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 sign prepub.example 2>&1 | sed 's/^/I:ns1 /'
newserial=$oldserial
try=0
while [ $oldserial -eq $newserial -a $try -lt 42 ]
do
	newserial=`$DIG $DIGOPTS +short soa prepub.example @10.53.0.3 |
		 awk '$0 !~ /SOA/ {print $3}'`
	sleep 1
	try=`expr $try + 1`
done
newinception=`$DIG $DIGOPTS +short soa prepub.example @10.53.0.3 | awk '/SOA/ {print $6}' | sort -u`
#echo "$oldserial : $newserial"
#echo "$oldinception : $newinception"

[ "$oldserial" = "$newserial" ] && ret=1
[ "$oldinception" = "$newinception" ] && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:preparing to test key change corner cases"
echo "I:removing a private key file"
file="ns1/`cat vanishing.key`.private"
rm -f $file

echo "I:preparing ZSK roll"
starttime=`$PERL -e 'print time(), "\n";'`
oldfile=`cat active.key`
oldid=`sed 's/^K.+007+0*\([0-9]\)/\1/' < active.key`
newfile=`cat standby.key`
newid=`sed 's/^K.+007+0*\([0-9]\)/\1/' < standby.key`
$SETTIME -K ns1 -I now+2s -D now+25 $oldfile > /dev/null
$SETTIME -K ns1 -i 0 -S $oldfile $newfile > /dev/null

# note previous zone serial number
oldserial=`$DIG $DIGOPTS +short soa . @10.53.0.1 | awk '{print $3}'`

$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 loadkeys . 2>&1 | sed 's/^/I:ns1 /'
sleep 4

echo "I:revoking key to duplicated key ID"
$SETTIME -R now -K ns2 Kbar.+005+30676.key > /dev/null

$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 loadkeys bar. 2>&1 | sed 's/^/I:ns2 /'

echo "I:waiting for changes to take effect"
sleep 5

echo "I:checking former standby key is now active ($n)"
ret=0
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $newid "'\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking former standby key has only signed incrementally ($n)"
ret=0
$DIG $DIGOPTS txt . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $newid "'\. ' dig.out.ns1.test$n > /dev/null && ret=1
grep 'RRSIG.*'" $oldid "'\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that signing records have been marked as complete ($n)"
ret=0
checkprivate . 10.53.0.1 || ret=1
checkprivate bar 10.53.0.2 || ret=1
checkprivate example 10.53.0.2 || ret=1
checkprivate private.secure.example 10.53.0.3 || ret=1
checkprivate nsec3.example 10.53.0.3 || ret=1
checkprivate nsec3.nsec3.example 10.53.0.3 || ret=1
checkprivate nsec3.optout.example 10.53.0.3 || ret=1
checkprivate nsec3-to-nsec.example 10.53.0.3 || ret=1
checkprivate nsec.example 10.53.0.3 || ret=1
checkprivate oldsigs.example 10.53.0.3 || ret=1
checkprivate optout.example 10.53.0.3 || ret=1
checkprivate optout.nsec3.example 10.53.0.3 || ret=1
checkprivate optout.optout.example 10.53.0.3 || ret=1
checkprivate prepub.example 10.53.0.3 || ret=1
checkprivate rsasha256.example 10.53.0.3 || ret=1
checkprivate rsasha512.example 10.53.0.3 || ret=1
checkprivate secure.example 10.53.0.3 || ret=1
checkprivate secure.nsec3.example 10.53.0.3 || ret=1
checkprivate secure.optout.example 10.53.0.3 || ret=1
checkprivate secure-to-insecure2.example 10.53.0.3 || ret=1
checkprivate secure-to-insecure.example 10.53.0.3 || ret=1
checkprivate ttl1.example 10.53.0.3 || ret=1
checkprivate ttl2.example 10.53.0.3 || ret=1
checkprivate ttl3.example 10.53.0.3 || ret=1
checkprivate ttl4.example 10.53.0.3 || ret=1
n=`expr $n + 1`
status=`expr $status + $ret`

echo "I:forcing full sign"
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 sign . 2>&1 | sed 's/^/I:ns1 /'

echo "I:waiting for change to take effect"
sleep 5

echo "I:checking former standby key has now signed fully ($n)"
ret=0
$DIG $DIGOPTS txt . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $newid "'\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking SOA serial number has been incremented ($n)"
ret=0
newserial=`$DIG $DIGOPTS +short soa . @10.53.0.1 | awk '{print $3}'`
[ "$newserial" != "$oldserial" ] || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking delayed key publication/activation ($n)"
ret=0
zsk=`cat delayzsk.key`
ksk=`cat delayksk.key`
# publication and activation times should be unset
$SETTIME -K ns3 -pA -pP $zsk | grep -v UNSET > /dev/null 2>&1 && ret=1
$SETTIME -K ns3 -pA -pP $ksk | grep -v UNSET > /dev/null 2>&1 && ret=1
$DIG $DIGOPTS +noall +answer dnskey delay.example. @10.53.0.3 > dig.out.ns3.test$n || ret=1
# DNSKEY not expected:
awk 'BEGIN {r=1} $4=="DNSKEY" {r=0} END {exit r}' dig.out.ns3.test$n && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking scheduled key publication, not activation ($n)"
ret=0
$SETTIME -K ns3 -P now+3s -A none $zsk > /dev/null 2>&1
$SETTIME -K ns3 -P now+3s -A none $ksk > /dev/null 2>&1
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 loadkeys delay.example. 2>&1 | sed 's/^/I:ns2 /'

echo "I:waiting for changes to take effect"
sleep 5

$DIG $DIGOPTS +noall +answer dnskey delay.example. @10.53.0.3 > dig.out.ns3.test$n || ret=1
# DNSKEY expected:
awk 'BEGIN {r=1} $4=="DNSKEY" {r=0} END {exit r}' dig.out.ns3.test$n || ret=1
# RRSIG not expected:
awk 'BEGIN {r=1} $4=="RRSIG" {r=0} END {exit r}' dig.out.ns3.test$n && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking scheduled key activation ($n)"
ret=0
$SETTIME -K ns3 -A now+3s $zsk > /dev/null 2>&1
$SETTIME -K ns3 -A now+3s $ksk > /dev/null 2>&1
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 loadkeys delay.example. 2>&1 | sed 's/^/I:ns2 /'

echo "I:waiting for changes to take effect"
sleep 5

$DIG $DIGOPTS +noall +answer dnskey delay.example. @10.53.0.3 > dig.out.ns3.1.test$n || ret=1
# DNSKEY expected:
awk 'BEGIN {r=1} $4=="DNSKEY" {r=0} END {exit r}' dig.out.ns3.1.test$n || ret=1
# RRSIG expected:
awk 'BEGIN {r=1} $4=="RRSIG" {r=0} END {exit r}' dig.out.ns3.1.test$n || ret=1
$DIG $DIGOPTS +noall +answer a a.delay.example. @10.53.0.3 > dig.out.ns3.2.test$n || ret=1
# A expected:
awk 'BEGIN {r=1} $4=="A" {r=0} END {exit r}' dig.out.ns3.2.test$n || ret=1
# RRSIG expected:
awk 'BEGIN {r=1} $4=="RRSIG" {r=0} END {exit r}' dig.out.ns3.2.test$n || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking former active key was removed ($n)"
#
# Work out how long we need to sleep. Allow 4 seconds for the records
# to be removed.
#
now=`$PERL -e 'print time(), "\n";'`
sleep=`expr $starttime + 29 - $now`
case $sleep in
-*|0);;
*) echo "I:waiting for timer to have activated"; sleep $sleep;;
esac
ret=0
$DIG $DIGOPTS +multi dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep '; key id =.*'"$oldid"'$' dig.out.ns1.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking private key file removal caused no immediate harm ($n)"
ret=0
id=`sed 's/^K.+007+0*\([0-9]\)/\1/' < vanishing.key`
$DIG $DIGOPTS dnskey . @10.53.0.1 > dig.out.ns1.test$n || ret=1
grep 'RRSIG.*'" $id "'\. ' dig.out.ns1.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking revoked key with duplicate key ID (failure expected) ($n)"
lret=0
id=30676
$DIG $DIGOPTS +multi dnskey bar @10.53.0.2 > dig.out.ns2.test$n || lret=1
grep '; key id =.*'"$id"'$' dig.out.ns2.test$n > /dev/null || lret=1
$DIG $DIGOPTS dnskey bar @10.53.0.4 > dig.out.ns4.test$n || lret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || lret=1
n=`expr $n + 1`
if [ $lret != 0 ]; then echo "I:not yet implemented"; fi

echo "I:checking key event timers are always set ($n)"
# this is a regression test for a bug in which the next key event could
# be scheduled for the present moment, and then never fire.  check for
# visible evidence of this error in the logs:
awk '/next key event/ {if ($1 == $8 && $2 == $9) exit 1}' */named.run || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# this confirms that key events are never scheduled more than
# a given number of seconds into the future, and that the last
# event scheduled is precisely that far in the future.
check_interval () {
        awk '/next key event/ {print $2 ":" $9}' $1/named.run |
	sed 's/\.//g' |
            awk -F: '
                     {
                       x = ($6+ $5*60000 + $4*3600000) - ($3+ $2*60000 + $1*3600000);
		       # abs(x) < 500 ms treat as 'now'
		       if (x < 500 && x > -500)
                         x = 0;
		       # convert to seconds
		       x = x/1000;
		       # handle end of day roll over
		       if (x < 0)
			 x = x + 24*3600;
		       # handle log timestamp being a few milliseconds later
                       if (x != int(x))
                         x = int(x + 1);
                       if (int(x) > int(interval))
                         exit (1);
                     }
                     END { if (int(x) != int(interval)) exit(1) }' interval=$2
        return $?
}

echo "I:checking automatic key reloading interval ($n)"
ret=0
check_interval ns1 3600 || ret=1
check_interval ns2 3600 || ret=1
check_interval ns3 3600 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"

exit $status
