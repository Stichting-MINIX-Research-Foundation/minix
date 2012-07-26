#!/bin/sh
#
# Copyright (C) 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.7 2010-01-18 19:19:31 each Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

# replace_data dname RR old_data new_data
replace_data()
{
	if [ $# -ne 4 ]; then
		echo I:unexpected input for replace_data
		return 1
	fi

	_dname=$1
	_rr=$2
	_olddata=$3
	_newdata=$4

	_ret=0
	$NSUPDATE -d <<END>> nsupdate.out.test 2>&1 || _ret=1
server 10.53.0.2 5300
update delete ${_dname} 30 ${_rr} ${_olddata}
update add ${_dname} 30 ${_rr} ${_newdata}
send
END

	if [ $_ret != 0 ]; then
		echo I:failed to update the test data
		return 1
	fi

	return 0
}

status=0
n=0

DIGOPTS="+short +tcp -p 5300"
DIGOPTS_CD="$DIGOPTS +cd"

echo I:Priming cache.
ret=0
expect="10 mail.example."
ans=`$DIG $DIGOPTS_CD @10.53.0.4 hostile MX` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo I:Checking that bogus additional is not returned with +CD.
ret=0
expect="10.0.0.2"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 mail.example A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

#
# Prime cache with pending additional records.  These should not be promoted
# to answer.
#
echo "I:Priming cache (pending additional A and AAAA)"
ret=0
expect="10 mail.example.com."
ans=`$DIG $DIGOPTS @10.53.0.4 example.com MX` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo "I:Replacing pending A"
ret=0
replace_data mail.example.com. A 192.0.2.2 192.0.2.3 || ret=1
status=`expr $status + $ret`

echo "I:Replacing pending AAAA"
ret=0
replace_data mail.example.com. AAAA 2001:db8::2 2001:db8::3 || ret=1
status=`expr $status + $ret`

echo "I:Checking updated data to be returned (without CD)"
ret=0
expect="192.0.2.3"
ans=`$DIG $DIGOPTS @10.53.0.4 mail.example.com A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo "I:Checking updated data to be returned (with CD)" 
ret=0
expect="2001:db8::3"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 mail.example.com AAAA` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

#
# Prime cache with a pending answer record.  It can be returned (without
# validation) with +CD.
#
echo "I:Priming cache (pending answer)"
ret=0
expect="192.0.2.2"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 pending-ok.example.com A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo I:Replacing pending data
ret=0
replace_data pending-ok.example.com. A 192.0.2.2 192.0.2.3 || ret=1
status=`expr $status + $ret`

echo I:Confirming cached pending data to be returned with CD
ret=0
expect="192.0.2.2"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 pending-ok.example.com A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

#
# Prime cache with a pending answer record.  It should not be returned
# to no-DNSSEC clients.
#
echo "I:Priming cache (pending answer)"
ret=0
expect="192.0.2.102"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 pending-ng.example.com A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo I:Replacing pending data
ret=0
replace_data pending-ng.example.com. A 192.0.2.102 192.0.2.103 || ret=1
status=`expr $status + $ret`

echo I:Confirming updated data returned, not the cached one, without CD
ret=0
expect="192.0.2.103"
ans=`$DIG $DIGOPTS @10.53.0.4 pending-ng.example.com A` || ret=1
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

#
# Try to fool the resolver with an out-of-bailiwick CNAME
#
echo I:Trying to Prime out-of-bailiwick pending answer with CD
ret=0
expect="10.10.10.10"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 bad.example. A` || ret=1
ans=`echo $ans | awk '{print $NF}'`
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo I:Confirming the out-of-bailiwick answer is not cached or reused with CD
ret=0
expect="10.10.10.10"
ans=`$DIG $DIGOPTS_CD @10.53.0.4 nice.good. A` || ret=1
ans=`echo $ans | awk '{print $NF}'`
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

#
# Make sure the resolver doesn't cache bogus NXDOMAIN
#
echo I:Trying to Prime bogus NXDOMAIN
ret=0
expect="SERVFAIL"
ans=`$DIG +tcp -p 5300 @10.53.0.4 removed.example.com. A` || ret=1
ans=`echo $ans | sed 's/^.*status: \([A-Z][A-Z]*\).*$/\1/'`
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo I:Confirming the bogus NXDOMAIN was not cached
ret=0
expect="SERVFAIL"
ans=`$DIG +tcp -p 5300 @10.53.0.4 removed.example.com. A` || ret=1
ans=`echo $ans | sed 's/^.*status: \([A-Z][A-Z]*\).*$/\1/'`
test "$ans" = "$expect" || ret=1
test $ret = 0 || echo I:failed, got "'""$ans""'", expected "'""$expect""'"
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
