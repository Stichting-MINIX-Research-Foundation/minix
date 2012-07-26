#!/bin/sh
#
# Copyright (C) 2004-2011  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2002  Internet Software Consortium.
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

# $Id: tests.sh,v 1.73.14.12 2011-05-26 04:25:08 each Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=1

rm -f dig.out.*

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

# Check the example. domain

echo "I:checking that zone transfer worked ($n)"
ret=0
$DIG $DIGOPTS a.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS a.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns3.test$n || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# test AD bit:
#  - dig +adflag asks for authentication (ad in response)
echo "I:checking AD bit asking for validation ($n)"
ret=0
$DIG $DIGOPTS +noauth +noadd +nodnssec +adflag a.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth +noadd +nodnssec +adflag a.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for AD in authoritative answer ($n)"
ret=0
$DIG $DIGOPTS a.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns2.test$n > /dev/null && ret=1
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

echo "I:checking positive wildcard validation NSEC ($n)"
ret=0
$DIG $DIGOPTS a.wild.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS a.wild.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive wildcard validation NSEC3 ($n)"
ret=0
$DIG $DIGOPTS a.wild.nsec3.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS a.wild.nsec3.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking positive wildcard validation OPTOUT ($n)"
ret=0
$DIG $DIGOPTS a.wild.optout.example. \
	@10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS a.wild.optout.example. \
	@10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
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

echo "I:checking negative wildcard validation NSEC ($n)"
ret=0
$DIG $DIGOPTS b.wild.example. @10.53.0.2 txt > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS b.wild.example. @10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative wildcard validation NSEC3 ($n)"
ret=0
$DIG $DIGOPTS b.wild.nsec3.example. @10.53.0.3 txt > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS b.wild.nsec3.example. @10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking negative wildcard validation OPTOUT ($n)"
ret=0
$DIG $DIGOPTS b.wild.optout.example. \
	@10.53.0.3 txt > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS b.wild.optout.example. \
	@10.53.0.4 txt > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
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

echo "I:checking 1-server insecurity proof NSEC3 ($n)"
ret=0
$DIG $DIGOPTS +noauth a.insecure.nsec3.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.insecure.nsec3.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server insecurity proof OPTOUT ($n)"
ret=0
$DIG $DIGOPTS +noauth a.insecure.optout.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.insecure.optout.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
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

echo "I:checking 1-server negative insecurity proof NSEC3 ($n)"
ret=0
$DIG $DIGOPTS q.insecure.nsec3.example. a @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS q.insecure.nsec3.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server negative insecurity proof OPTOUT ($n)"
ret=0
$DIG $DIGOPTS q.insecure.optout.example. a @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS q.insecure.optout.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server negative insecurity proof with SOA hack NSEC ($n)"
ret=0
$DIG $DIGOPTS r.insecure.example. soa @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS r.insecure.example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
grep "0	IN	SOA" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server negative insecurity proof with SOA hack NSEC3 ($n)"
ret=0
$DIG $DIGOPTS r.insecure.nsec3.example. soa @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS r.insecure.nsec3.example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
grep "0	IN	SOA" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking 1-server negative insecurity proof with SOA hack OPTOUT ($n)"
ret=0
$DIG $DIGOPTS r.insecure.optout.example. soa @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS r.insecure.optout.example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
grep "0	IN	SOA" dig.out.ns4.test$n > /dev/null || ret=1
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

# Check the bogus domain

echo "I:checking failed validation ($n)"
ret=0
$DIG $DIGOPTS a.bogus.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
grep "SERVFAIL" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Try validating with a bad trusted key.
# This should fail.

echo "I:checking that validation fails with a misconfigured trusted key ($n)"
ret=0
$DIG $DIGOPTS example. soa @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "SERVFAIL" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that negative validation fails with a misconfigured trusted key ($n)"
ret=0
$DIG $DIGOPTS example. ptr @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "SERVFAIL" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that insecurity proofs fail with a misconfigured trusted key ($n)"
ret=0
$DIG $DIGOPTS a.insecure.example. a @10.53.0.5 > dig.out.ns5.test$n || ret=1
grep "SERVFAIL" dig.out.ns5.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation fails when key record is missing ($n)"
ret=0
$DIG $DIGOPTS a.b.keyless.example. a @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "SERVFAIL" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:Checking that a bad CNAME signature is caught after a +CD query ($n)"
ret=0
#prime
$DIG $DIGOPTS +cd bad-cname.example. @10.53.0.4 > dig.out.ns4.prime$n || ret=1
#check: requery with +CD.  pending data should be returned even if it's bogus
expect="a.example.
10.0.0.1"
ans=`$DIG $DIGOPTS +cd +nodnssec +short bad-cname.example. @10.53.0.4` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
#check: requery without +CD.  bogus cached data should be rejected.
$DIG $DIGOPTS +nodnssec bad-cname.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "SERVFAIL" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:Checking that a bad DNAME signature is caught after a +CD query ($n)"
ret=0
#prime
$DIG $DIGOPTS +cd a.bad-dname.example. @10.53.0.4 > dig.out.ns4.prime$n || ret=1
#check: requery with +CD.  pending data should be returned even if it's bogus
expect="example.
a.example.
10.0.0.1"
ans=`$DIG $DIGOPTS +cd +nodnssec +short a.bad-dname.example. @10.53.0.4` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
#check: requery without +CD.  bogus cached data should be rejected.
$DIG $DIGOPTS +nodnssec a.bad-dname.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "SERVFAIL" dig.out.ns4.test$n > /dev/null || ret=1
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

echo "I:checking 2-server insecurity proof with a negative answer and SOA hack ($n)"
ret=0
$DIG $DIGOPTS r.insecure.secure.example. @10.53.0.2 soa > dig.out.ns2.test$n \
	|| ret=1
$DIG $DIGOPTS r.insecure.secure.example. @10.53.0.4 soa > dig.out.ns4.test$n \
	|| ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check that the query for a security root is successful and has ad set

echo "I:checking security root query ($n)"
ret=0
$DIG $DIGOPTS . @10.53.0.4 key > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check that the setting the cd bit works

echo "I:checking cd bit on a positive answer ($n)"
ret=0
$DIG $DIGOPTS +noauth example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$DIG $DIGOPTS +noauth +cdflag example. soa @10.53.0.5 \
	> dig.out.ns5.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns4.test$n dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking cd bit on a negative answer ($n)"
ret=0
$DIG $DIGOPTS q.example. soa @10.53.0.4 > dig.out.ns4.test$n || ret=1
$DIG $DIGOPTS +cdflag q.example. soa @10.53.0.5 > dig.out.ns5.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns4.test$n dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null && ret=1
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

echo "I:checking positive validation with KSK-only DNSKEY signature ($n)"
ret=0
$DIG $DIGOPTS +noauth a.kskonly.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +noauth a.kskonly.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns3.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking cd bit on a query that should fail ($n)"
ret=0
$DIG $DIGOPTS a.bogus.example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$DIG $DIGOPTS +cdflag a.bogus.example. soa @10.53.0.5 \
	> dig.out.ns5.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns4.test$n dig.out.ns5.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking cd bit on an insecurity proof ($n)"
ret=0
$DIG $DIGOPTS +noauth a.insecure.example. soa @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$DIG $DIGOPTS +noauth +cdflag a.insecure.example. soa @10.53.0.5 \
	> dig.out.ns5.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns4.test$n dig.out.ns5.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# Note - these are looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking cd bit on a negative insecurity proof ($n)"
ret=0
$DIG $DIGOPTS q.insecure.example. a @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$DIG $DIGOPTS +cdflag q.insecure.example. a @10.53.0.5 \
	> dig.out.ns5.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns4.test$n dig.out.ns5.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
# Note - these are looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
grep "flags:.*ad.*QUERY" dig.out.ns5.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation of an ANY query works ($n)"
ret=0
$DIG $DIGOPTS +noauth foo.example. any @10.53.0.2 > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth foo.example. any @10.53.0.4 > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# 2 records in the zone, 1 NXT, 3 SIGs
grep "ANSWER: 6" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation of a query returning a CNAME works ($n)"
ret=0
$DIG $DIGOPTS +noauth cname1.example. txt @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth cname1.example. txt @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# the CNAME & its sig, the TXT and its SIG
grep "ANSWER: 4" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation of a query returning a DNAME works ($n)"
ret=0
$DIG $DIGOPTS +noauth foo.dname1.example. txt @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth foo.dname1.example. txt @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# The DNAME & its sig, the TXT and its SIG, and the synthesized CNAME.
# It would be nice to test that the CNAME is being synthesized by the
# recursive server and not cached, but I don't know how.
grep "ANSWER: 5" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation of an ANY query returning a CNAME works ($n)"
ret=0
$DIG $DIGOPTS +noauth cname2.example. any @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth cname2.example. any @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
# The CNAME, NXT, and their SIGs
grep "ANSWER: 4" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that validation of an ANY query returning a DNAME works ($n)"
ret=0
$DIG $DIGOPTS +noauth foo.dname2.example. any @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth foo.dname2.example. any @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
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

echo "I:checking that lookups succeed after disabling a algorithm works ($n)"
ret=0
$DIG $DIGOPTS +noauth example. SOA @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth example. SOA @10.53.0.6 \
	> dig.out.ns6.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns6.test$n || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns6.test$n > /dev/null && ret=1
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

echo "I:checking privately secure wildcard to nxdomain works ($n)"
ret=0
$DIG $DIGOPTS +noauth a.wild.private.secure.example. SOA @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth a.wild.private.secure.example. SOA @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
# Note - this is looking for failure, hence the &&
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking a non-cachable NODATA works ($n)"
ret=0
$DIG $DIGOPTS +noauth a.nosoa.secure.example. txt @10.53.0.7 \
	> dig.out.ns7.test$n || ret=1
grep "AUTHORITY: 0" dig.out.ns7.test$n > /dev/null || ret=1
$DIG $DIGOPTS +noauth a.nosoa.secure.example. txt @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
grep "status: NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking a non-cachable NXDOMAIN works ($n)"
ret=0
$DIG $DIGOPTS +noauth b.nosoa.secure.example. txt @10.53.0.7 \
	> dig.out.ns7.test$n || ret=1
grep "AUTHORITY: 0" dig.out.ns7.test$n > /dev/null || ret=1
$DIG $DIGOPTS +noauth b.nosoa.secure.example. txt @10.53.0.4 \
	> dig.out.ns4.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

#
# private.secure.example is served by the same server as its
# grand parent and there is not a secure delegation from secure.example
# to private.secure.example.  In addition secure.example is using a
# algorithm which the validation does not support.
#
echo "I:checking dnssec-lookaside-validation works ($n)"
ret=0
$DIG $DIGOPTS private.secure.example. SOA @10.53.0.6 \
	> dig.out.ns6.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns6.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that we can load a rfc2535 signed zone ($n)"
ret=0
$DIG $DIGOPTS rfc2535.example. SOA @10.53.0.2 \
	> dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that we can transfer a rfc2535 signed zone ($n)"
ret=0
$DIG $DIGOPTS rfc2535.example. SOA @10.53.0.3 \
	> dig.out.ns3.test$n || ret=1
grep "status: NOERROR" dig.out.ns3.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that we can sign a zone with out-of-zone records ($n)"
ret=0
(
cd signer
RANDFILE=../random.data
zone=example
key1=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone`
key2=`$KEYGEN -q -r $RANDFILE -f KSK -a NSEC3RSASHA1 -b 1024 -n zone $zone`
cat example.db.in $key1.key $key2.key > example.db
$SIGNER -o example -f example.db example.db > /dev/null 2>&1
) || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that we can sign a zone (NSEC3) with out-of-zone records ($n)"
ret=0
(
cd signer
RANDFILE=../random.data
zone=example
key1=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 1024 -n zone $zone`
key2=`$KEYGEN -q -r $RANDFILE -f KSK -a NSEC3RSASHA1 -b 1024 -n zone $zone`
cat example.db.in $key1.key $key2.key > example.db
$SIGNER -3 - -H 10 -o example -f example.db example.db > /dev/null 2>&1
grep "IQF9LQTLKKNFK0KVIFELRAK4IC4QLTMG.example. 0 IN NSEC3 1 0 10 - IQF9LQTLKKNFK0KVIFELRAK4IC4QLTMG A NS SOA RRSIG DNSKEY NSEC3PARAM" example.db > /dev/null 
) || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that dnsssec-signzone updates originalttl on ttl changes ($n)"
ret=0
(
cd signer
RANDFILE=../random.data
zone=example
key1=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone`
key2=`$KEYGEN -q -r $RANDFILE -f KSK -a RSASHA1 -b 1024 -n zone $zone`
cat example.db.in $key1.key $key2.key > example.db
$SIGNER -o example -f example.db.before example.db > /dev/null 2>&1
sed 's/60.IN.SOA./50 IN SOA /' example.db.before > example.db.changed
$SIGNER -o example -f example.db.after example.db.changed > /dev/null 2>&1
)
grep "SOA 5 1 50" signer/example.db.after > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking validated data are not cached longer than originalttl ($n)"
ret=0
$DIG $DIGOPTS +ttl +noauth a.ttlpatch.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
$DIG $DIGOPTS +ttl +noauth a.ttlpatch.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
grep "3600.IN" dig.out.ns3.test$n > /dev/null || ret=1
grep "300.IN" dig.out.ns3.test$n > /dev/null && ret=1
grep "300.IN" dig.out.ns4.test$n > /dev/null || ret=1
grep "3600.IN" dig.out.ns4.test$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Test that "rndc secroots" is able to dump trusted keys
echo "I:checking rndc secroots ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.4 -p 9953 secroots 2>&1 | sed 's/^/I:ns1 /'
keyid=`cat ns1/managed.key.id`
linecount=`grep "./RSAMD5/$keyid ; trusted" ns4/named.secroots | wc -l`
[ "$linecount" -eq 1 ] || ret=1
linecount=`cat ns4/named.secroots | wc -l`
[ "$linecount" -eq 5 ] || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check direct query for RRSIG.  If we first ask for normal (non RRSIG)
# record, the corresponding RRSIG should be cached and subsequent query
# for RRSIG will be returned with the cached record.
echo "I:checking RRSIG query from cache ($n)"
ret=0
$DIG $DIGOPTS normalthenrrsig.secure.example. @10.53.0.4 a > /dev/null || ret=1
ans=`$DIG $DIGOPTS +short normalthenrrsig.secure.example. @10.53.0.4 rrsig` || ret=1
expect=`$DIG $DIGOPTS +short normalthenrrsig.secure.example. @10.53.0.3 rrsig | grep '^A' ` || ret=1
test "$ans" = "$expect" || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Check direct query for RRSIG: If it's not cached with other records,
# it should result in an empty response.
echo "I:checking RRSIG query not in cache ($n)"
ret=0
ans=`$DIG $DIGOPTS +short rrsigonly.secure.example. @10.53.0.4 rrsig` || ret=1
test -z "$ans" || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

#
# RT21868 regression test.
#
echo "I:checking NSEC3 zone with mismatched NSEC3PARAM / NSEC parameters ($n)"
ret=0
$DIG $DIGOPTS non-exist.badparam. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

#
# RT22007 regression test.
#
echo "I:checking optout NSEC3 referral with only insecure delegations ($n)"
ret=0
$DIG $DIGOPTS +norec delegation.single-nsec3. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN.*NSEC3 1 1 1 - 3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking optout NSEC3 NXDOMAIN with only insecure delegations ($n)"
ret=0
$DIG $DIGOPTS +norec nonexist.single-nsec3. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NXDOMAIN" dig.out.ns2.test$n > /dev/null || ret=1
grep "3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN.*NSEC3 1 1 1 - 3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi

status=`expr $status + $ret`
echo "I:checking optout NSEC3 nodata with only insecure delegations ($n)"
ret=0
$DIG $DIGOPTS +norec single-nsec3. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
grep "status: NOERROR" dig.out.ns2.test$n > /dev/null || ret=1
grep "3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN.*NSEC3 1 1 1 - 3KL3NK1HKQ4IUEEHBEF12VGFKUETNBAN" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a zone finishing the transition from RSASHA1 to RSASHA256 validates secure ($n)"
ret=0
$DIG $DIGOPTS ns algroll. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:[^;]* ad[^;]*;" dig.out.ns4.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

# Run a minimal update test if possible.  This is really just
# a regression test for RT #2399; more tests should be added.

if $PERL -e 'use Net::DNS;' 2>/dev/null
then
    echo "I:running DNSSEC update test"
    $PERL dnssec_update_test.pl -s 10.53.0.3 -p 5300 dynamic.example. || status=1
else
    echo "I:The DNSSEC update test requires the Net::DNS library." >&2
fi

# Reconfigure caching server to use "dnssec-validation auto", and repeat
# some of the DNSSEC validation tests to ensure that it works correctly.
echo "I:switching to automatic root key configuration"
cp ns4/named2.conf ns4/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.4 -p 9953 reconfig 2>&1 | sed 's/^/I:ns4 /'
sleep 5

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

echo "I:checking negative validation ($n)"
ret=0
$DIG $DIGOPTS +noauth q.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth q.example. @10.53.0.4 a > dig.out.ns4.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns4.test$n || ret=1
grep "flags:.*ad.*QUERY" dig.out.ns4.test$n > /dev/null || ret=1
grep "status: NXDOMAIN" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking expired signatures remain with "'"allow-update { none; };"'" and no keys available ($n)"
ret=0
$DIG $DIGOPTS +noauth expired.example. +dnssec @10.53.0.3 soa > dig.out.ns2.test$n || ret=1
grep "RRSIG.SOA" dig.out.ns2.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that the NSEC3 record for the apex is properly signed when a DNSKEY is added via UPDATE ($n)"
ret=0
(
cd ns3
kskname=`$KEYGEN -q -3 -r ../random.data -fk update-nsec3.example`
(
echo zone update-nsec3.example
echo server 10.53.0.3 5300
grep DNSKEY ${kskname}.key | sed -e 's/^/update add /' -e 's/IN/300 IN/'
echo send
) | $NSUPDATE
)
$DIG $DIGOPTS +dnssec a update-nsec3.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns4.test$n > /dev/null || ret=1
grep "NSEC3 .* TYPE65534" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that the NSEC record is properly generated when DNSKEY are added via auto-dnssec ($n)"
ret=0
$DIG $DIGOPTS +dnssec a auto-nsec.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns4.test$n > /dev/null || ret=1
grep "IN.NSEC[^3].* DNSKEY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that the NSEC3 record is properly generated when DNSKEY are added via auto-dnssec ($n)"
ret=0
$DIG $DIGOPTS +dnssec a auto-nsec3.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns4.test$n > /dev/null || ret=1
grep "IN.NSEC3 .* DNSKEY" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that signing records have been marked as complete ($n)"
ret=0
checkprivate dynamic.example 10.53.0.3 || ret=1
checkprivate update-nsec3.example 10.53.0.3 || ret=1
checkprivate auto-nsec3.example 10.53.0.3 || ret=1
checkprivate expiring.example 10.53.0.3 || ret=1
checkprivate auto-nsec.example 10.53.0.3 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a insecure zone beneath a cname resolves ($n)"
ret=0
$DIG $DIGOPTS soa insecure.below-cname.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 1," dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a secure zone beneath a cname resolves ($n)"
ret=0
$DIG $DIGOPTS soa secure.below-cname.example. @10.53.0.4 > dig.out.ns4.test$n || ret=1
grep "NOERROR" dig.out.ns4.test$n > /dev/null || ret=1
grep "ANSWER: 2," dig.out.ns4.test$n > /dev/null || ret=1
grep "flags:.* ad[ ;]" dig.out.ns4.test$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking dnskey query with no data still gets put in cache ($n)"
ret=0
myDIGOPTS="+noadd +nosea +nostat +noquest +nocomm +nocmd -p 5300 @10.53.0.4"
firstVal=`$DIG $myDIGOPTS insecure.example. dnskey|awk '{ print $2 }'`
sleep 1
secondVal=`$DIG $myDIGOPTS insecure.example. dnskey|awk '{ print $2 }'`
if [ $firstVal -eq $secondVal ]
then
	sleep 1
	thirdVal=`$DIG $myDIGOPTS insecure.example. dnskey|awk '{ print $2 }'`
	if [ $firstVal -eq $thirdVal ]
	then
		echo "I: cannot confirm query answer still in cache"
		ret=1
	fi
fi
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check that NOTIFY is sent at the end of NSEC3 chain generation ($n)"
ret=0
(
echo zone nsec3chain-test
echo server 10.53.0.2 5300
echo update add nsec3chain-test. 0 nsec3param 1 0 1 123456
echo send
) | $NSUPDATE
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18
do
	$DIG $DIGOPTS nsec3param nsec3chain-test @10.53.0.2 > dig.out.ns2.test$n || ret=1
	if grep "ANSWER: 3," dig.out.ns2.test$n >/dev/null
	then
		break;
	fi
	echo "I:sleeping ...."
	sleep 3
done;
grep "ANSWER: 3," dig.out.ns2.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:nsec3 chain generation not complete"; fi
sleep 3
$DIG $DIGOPTS +noauth +nodnssec soa nsec3chain-test @10.53.0.2 > dig.out.ns2.test$n || ret=1
$DIG $DIGOPTS +noauth +nodnssec soa nsec3chain-test @10.53.0.3 > dig.out.ns3.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns2.test$n dig.out.ns3.test$n || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:testing soon-to-expire RRSIGs without a replacement private key ($n)"
ret=0
$DIG +noall +answer +dnssec +nottl -p 5300 expiring.example ns @10.53.0.3 | grep RRSIG > dig.out.ns3.test$n 2>&1
# there must be a signature here
[ -s dig.out.ns3.test$n ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
