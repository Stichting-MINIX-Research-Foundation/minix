#!/bin/sh
#
# Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.1.4.11 2012/02/01 16:54:32 each Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"
DIGCMD="$DIG $DIGOPTS -p 5300"
RNDCCMD="$RNDC -p 9953 -c ../common/rndc.conf"

status=0
ret=0
echo "I:fetching a.example from ns2's initial configuration"
$DIGCMD +noauth a.example. @10.53.0.2 any > dig.out.ns2.1 || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I:verifying adb records in named.stats"
$RNDCCMD -s 10.53.0.2 stats > /dev/null 2>&1
echo "I: checking for 1 entry in adb hash table"
grep "1 Addresses in hash table" ns2/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: verifying cache statistics in named.stats"
grep "Cache Statistics" ns2/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: checking for 2 entries in adb hash table"
$DIGCMD a.example.info. @10.53.0.2 any > /dev/null 2>&1
$RNDCCMD -s 10.53.0.2 stats > /dev/null 2>&1
grep "2 Addresses in hash table" ns2/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I:dumping initial stats for ns3"
rm -f ns3/named.stats
$RNDCCMD -s 10.53.0.3 stats > /dev/null 2>&1
[ -f ns3/named.stats ] || ret=1
nsock0=`grep "UDP/IPv4 sockets active" ns3/named.stats | awk '{print $1}'`
echo "I:sending queries to ns3"
$DIGCMD +tries=2 +time=1 +recurse @10.53.0.3 foo.info. any > /dev/null 2>&1
#$DIGCMD +tries=2 +time=1 +recurse @10.53.0.3 foo.info. any
echo "I:dumping updated stats for ns3"
rm -f ns3/named.stats
$RNDCCMD -s 10.53.0.3 stats > /dev/null 2>&1
[ -f ns3/named.stats ] || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: verifying recursing clients output"
grep "2 recursing clients" ns3/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: verifying active fetches output"
grep "1 active fetches" ns3/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: verifying active sockets output"
nsock1=`grep "UDP/IPv4 sockets active" ns3/named.stats | awk '{print $1}'`
[ `expr $nsock1 - $nsock0` -eq 1 ] || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
# there should be 1 UDP and no TCP queries.  As the TCP counter is zero
# no status line is emitted.
echo "I: verifying queries in progress"
grep "1 UDP queries in progress" ns3/named.stats > /dev/null || ret=1
grep "TCP queries in progress" ns3/named.stats > /dev/null && ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

ret=0
echo "I: verifying bucket size output"
grep "bucket size" ns3/named.stats > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I: failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
