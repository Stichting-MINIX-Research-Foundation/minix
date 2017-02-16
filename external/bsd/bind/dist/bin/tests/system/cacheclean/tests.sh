#!/bin/sh
#
# Copyright (C) 2004, 2007, 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.10 2011/09/01 05:28:14 marka Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

RNDCOPTS="-c ../common/rndc.conf -s 10.53.0.2 -p 9953"
DIGOPTS="+nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm \
         +nostat @10.53.0.2 -p 5300"

# fill the cache with nodes from flushtest.example zone
load_cache () {
        # empty all existing cache data
        $RNDC $RNDCOPTS flush

	# load the positive cache entries
	$DIG $DIGOPTS -f - << EOF > /dev/null 2>&1
txt top1.flushtest.example
txt second1.top1.flushtest.example
txt third1.second1.top1.flushtest.example
txt third2.second1.top1.flushtest.example
txt second2.top1.flushtest.example
txt second3.top1.flushtest.example
txt second1.top2.flushtest.example
txt second2.top2.flushtest.example
txt second3.top2.flushtest.example
txt top3.flushtest.example
txt second1.top3.flushtest.example
txt third1.second1.top3.flushtest.example
txt third2.second1.top3.flushtest.example
txt third1.second2.top3.flushtest.example
txt third2.second2.top3.flushtest.example
txt second3.top3.flushtest.example
EOF

	# load the negative cache entries
        # nxrrset:
	$DIG $DIGOPTS a third1.second1.top1.flushtest.example > /dev/null
        # nxdomain:
	$DIG $DIGOPTS txt top4.flushtest.example > /dev/null
        # empty nonterminal:
	$DIG $DIGOPTS txt second2.top3.flushtest.example > /dev/null

	# sleep 2 seconds ensure the TTLs will be lower on cached data
	sleep 2
}

dump_cache () {
        rm -f ns2/named_dump.db
        $RNDC $RNDCOPTS dumpdb -cache _default
        sleep 1
}

clear_cache () {
        $RNDC $RNDCOPTS flush
}

in_cache () {
        ttl=`$DIG $DIGOPTS "$@" | awk '{print $2}'`
        [ -z "$ttl" ] && {
                ttl=`$DIG $DIGOPTS +noanswer +auth "$@" | awk '{print $2}'`
                [ "$ttl" -ge 3599 ] && return 1
                return 0
        }
        [ "$ttl" -ge 3599 ] && return 1
        return 0
}

echo "I:check correctness of routine cache cleaning"
$DIG $DIGOPTS +tcp +keepopen -b 10.53.0.7 -f dig.batch > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$PERL ../digcomp.pl --lc dig.out.ns2 knowngood.dig.out || status=1

echo "I:only one tcp socket was used"
tcpclients=`grep "client 10.53.0.7#[0-9]*:" ns2/named.run | awk '{print $4}' | sort | uniq -c | wc -l`

test $tcpclients -eq 1 || { status=1; echo "I:failed"; }

echo "I:reset and check that records are correctly cached initially"
ret=0
load_cache
dump_cache
nrecords=`grep flushtest.example ns2/named_dump.db | grep -v '^;' | egrep '(TXT|ANY)'|  wc -l`
[ $nrecords -eq 17 ] || { ret=1; echo "I: found $nrecords records expected 17"; }
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing of the full cache"
ret=0
clear_cache
dump_cache
nrecords=`grep flushtest.example ns2/named_dump.db | grep -v '^;' | wc -l`
[ $nrecords -eq 0 ] || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing of individual nodes (interior node)"
ret=0
clear_cache
load_cache
# interior node
in_cache txt top1.flushtest.example || ret=1
$RNDC $RNDCOPTS flushname top1.flushtest.example
in_cache txt top1.flushtest.example && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing of individual nodes (leaf node, under the interior node)"
ret=0
# leaf node, under the interior node (should still exist)
in_cache txt third2.second1.top1.flushtest.example || ret=1
$RNDC $RNDCOPTS flushname third2.second1.top1.flushtest.example
in_cache txt third2.second1.top1.flushtest.example && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing of individual nodes (another leaf node, with both positive and negative cache entries)"
ret=0
# another leaf node, with both positive and negative cache entries
in_cache a third1.second1.top1.flushtest.example || ret=1
in_cache txt third1.second1.top1.flushtest.example || ret=1
$RNDC $RNDCOPTS flushname third1.second1.top1.flushtest.example
in_cache a third1.second1.top1.flushtest.example && ret=1
in_cache txt third1.second1.top1.flushtest.example && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing a nonexistent name"
ret=0
$RNDC $RNDCOPTS flushname fake.flushtest.example || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing of namespaces"
ret=0
clear_cache
load_cache
# flushing leaf node should leave the interior node:
in_cache txt third1.second1.top1.flushtest.example || ret=1
in_cache txt top1.flushtest.example || ret=1
$RNDC $RNDCOPTS flushtree third1.second1.top1.flushtest.example
in_cache txt third1.second1.top1.flushtest.example && ret=1
in_cache txt top1.flushtest.example || ret=1
in_cache txt second1.top1.flushtest.example || ret=1
in_cache txt third2.second1.top1.flushtest.example || ret=1
$RNDC $RNDCOPTS flushtree second1.top1.flushtest.example
in_cache txt top1.flushtest.example || ret=1
in_cache txt second1.top1.flushtest.example && ret=1
in_cache txt third2.second1.top1.flushtest.example && ret=1

# flushing from an empty node should still remove all its children
in_cache txt second1.top2.flushtest.example || ret=1
$RNDC $RNDCOPTS flushtree top2.flushtest.example
in_cache txt second1.top2.flushtest.example && ret=1
in_cache txt second2.top2.flushtest.example && ret=1
in_cache txt second3.top2.flushtest.example && ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushing a nonexistent namespace"
ret=0
$RNDC $RNDCOPTS flushtree fake.flushtest.example || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check the number of cached records remaining"
ret=0
dump_cache
nrecords=`grep flushtest.example ns2/named_dump.db | grep -v '^;' | egrep '(TXT|ANY)' |  wc -l`
[ $nrecords -eq 17 ] || { ret=1; echo "I: found $nrecords records expected 17"; }
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check flushtree clears adb correctly"
ret=0
load_cache
dump_cache
awk '/plain success\/timeout/ {getline; getline; if ($2 == "ns.flushtest.example") exit(0); exit(1); }' ns2/named_dump.db || ret=1
$RNDC $RNDCOPTS flushtree flushtest.example || ret=1
dump_cache
awk '/plain success\/timeout/ {getline; getline; if ($2 == "ns.flushtest.example") exit(1); exit(0); }' ns2/named_dump.db || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:check expire option returned from slave zone"
ret=0
$DIG @10.53.0.2 -p 5300 +expire soa expire-test > dig.out.expire
grep EXPIRE: dig.out.expire > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
