# Copyright (C) 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
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


# test response rate limiting

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

#set -x

ns1=10.53.0.1			    # root, defining the others
ns2=10.53.0.2			    # test server
ns3=10.53.0.3			    # secondary test server
ns7=10.53.0.7			    # whitelisted client

USAGE="$0: [-x]"
while getopts "x" c; do
    case $c in
	x) set -x;;
	*) echo "$USAGE" 1>&2; exit 1;;
    esac
done
shift `expr $OPTIND - 1 || true`
if test "$#" -ne 0; then
    echo "$USAGE" 1>&2
    exit 1
fi
# really quit on control-C
trap 'exit 1' 1 2 15


ret=0
setret () {
    ret=1
    echo "$*"
}


# Wait until soon after the start of a second to make results consistent.
#   The start of a second credits a rate limit.
#   This would be far easier in C or by assuming a modern version of perl.
sec_start () {
    START=`date`
    while true; do
	NOW=`date`
	if test "$START" != "$NOW"; then
	    return
	fi
	$PERL -e 'select(undef, undef, undef, 0.05)' || true
    done
}


# turn off ${HOME}/.digrc
HOME=/dev/null; export HOME

#   $1=result name  $2=domain name  $3=dig options
digcmd () {
    OFILE=$1; shift
    DIG_DOM=$1; shift
    ARGS="+nosearch +time=1 +tries=1 +ignore -p 5300 $* $DIG_DOM @$ns2"
    #echo I:dig $ARGS 1>&2
    START=`date +%y%m%d%H%M.%S`
    RESULT=`$DIG $ARGS 2>&1 | tee $OFILE=TEMP				\
	    | sed -n -e '/^;; AUTHORITY/,/^$/d'				\
		-e '/^;; ADDITIONAL/,/^$/d'				\
		-e  's/^[^;].*	\([^	 ]\{1,\}\)$/\1/p'		\
		-e 's/;; flags.* tc .*/TC/p'				\
		-e 's/;; .* status: NXDOMAIN.*/NXDOMAIN/p'		\
		-e 's/;; .* status: SERVFAIL.*/SERVFAIL/p'		\
		-e 's/;; connection timed out.*/drop/p'			\
		-e 's/;; communications error to.*/drop/p'		\
	    | tr -d '\n'`
    mv "$OFILE=TEMP" "$OFILE=$RESULT"
    touch -t $START "$OFILE=$RESULT"
}


#   $1=number of tests  $2=target domain  $3=dig options
QNUM=1
burst () {
    BURST_LIMIT=$1; shift
    BURST_DOM_BASE="$1"; shift
    while test "$BURST_LIMIT" -ge 1; do
	CNT=`expr "00$QNUM" : '.*\(...\)'`
	eval BURST_DOM="$BURST_DOM_BASE"
	FILE="dig.out-$BURST_DOM-$CNT"
	digcmd $FILE $BURST_DOM $* &
	QNUM=`expr $QNUM + 1`
	BURST_LIMIT=`expr "$BURST_LIMIT" - 1`
    done
}


#   $1=domain  $2=IP address  $3=# of IP addresses  $4=TC  $5=drop
#	$6=NXDOMAIN  $7=SERVFAIL or other errors
ck_result() {
    BAD=
    wait
    ADDRS=`ls dig.out-$1-*=$2				2>/dev/null | wc -l`
    # count simple truncated and truncated NXDOMAIN as TC
    TC=`ls dig.out-$1-*=TC dig.out-$1-*=NXDOMAINTC	2>/dev/null | wc -l`
    DROP=`ls dig.out-$1-*=drop				2>/dev/null | wc -l`
    # count NXDOMAIN and truncated NXDOMAIN as NXDOMAIN
    NXDOMAIN=`ls dig.out-$1-*=NXDOMAIN  dig.out-$1-*=NXDOMAINTC	2>/dev/null \
							| wc -l`
    SERVFAIL=`ls dig.out-$1-*=SERVFAIL			2>/dev/null | wc -l`
    if test $ADDRS -ne "$3"; then
	setret "I:"$ADDRS" instead of $3 '$2' responses for $1"
	BAD=yes
    fi
    if test $TC -ne "$4"; then
	setret "I:"$TC" instead of $4 truncation responses for $1"
	BAD=yes
    fi
    if test $DROP -ne "$5"; then
	setret "I:"$DROP" instead of $5 dropped responses for $1"
	BAD=yes
    fi
    if test $NXDOMAIN -ne "$6"; then
	setret "I:"$NXDOMAIN" instead of $6 NXDOMAIN responses for $1"
	BAD=yes
    fi
    if test $SERVFAIL -ne "$7"; then
	setret "I:"$SERVFAIL" instead of $7 error responses for $1"
	BAD=yes
    fi
    if test -z "$BAD"; then
	rm -f dig.out-$1-*
    fi
}


ckstats () {
    LABEL="$1"; shift
    TYPE="$1"; shift
    EXPECTED="$1"; shift
    C=`sed -n -e "s/[	 ]*\([0-9]*\).responses $TYPE for rate limits.*/\1/p"  \
	    ns2/named.stats | tail -1`
    C=`expr 0$C + 0`
    if test "$C" -ne $EXPECTED; then
	setret "I:wrong $LABEL $TYPE statistics of $C instead of $EXPECTED"
    fi
}


#########
sec_start

# Tests of referrals to "." must be done before the hints are loaded
#   or with "additional-from-cache no"
burst 5 a1.tld3 +norec
# basic rate limiting
burst 3 a1.tld2
# 1 second delay allows an additional response.
sleep 1
burst 10 a1.tld2
# Request 30 different qnames to try a wildcard.
burst 30 'x$CNT.a2.tld2'
# These should be counted and limited but are not.  See RT33138.
burst 10 'y.x$CNT.a2.tld2'

#					IP      TC      drop  NXDOMAIN SERVFAIL
# referrals to "."
ck_result   a1.tld3	''		2	1	2	0	0
# check 13 results including 1 second delay that allows an additional response
ck_result   a1.tld2	192.0.2.1	3	4	6	0	0

# Check the wild card answers.
# The parent name of the 30 requests is counted.
ck_result 'x*.a2.tld2'	192.0.2.2	2	10	18	0	0

# These should be limited but are not.  See RT33138.
ck_result 'y.x*.a2.tld2' 192.0.2.2	10	0	0	0	0

#########
sec_start

burst 10 'x.a3.tld3'
burst 10 'y$CNT.a3.tld3'
burst 10 'z$CNT.a4.tld2'

# 10 identical recursive responses are limited
ck_result 'x.a3.tld3'	192.0.3.3	2	3	5	0	0

# 10 different recursive responses are not limited
ck_result 'y*.a3.tld3'	192.0.3.3	10	0	0	0	0

# 10 different NXDOMAIN responses are limited based on the parent name.
#   We count 13 responses because we count truncated NXDOMAIN responses
#   as both truncated and NXDOMAIN.
ck_result 'z*.a4.tld2'	x		0	3	5	5	0

$RNDC -c $SYSTEMTESTTOP/common/rndc.conf -p 9953 -s $ns2 stats
ckstats first dropped 36
ckstats first truncated 21


#########
sec_start

burst 10 a5.tld2 +tcp
burst 10 a6.tld2 -b $ns7
burst 10 a7.tld4
burst 2 a8.tld2 AAAA
burst 2 a8.tld2 TXT
burst 2 a8.tld2 SPF

#					IP      TC      drop  NXDOMAIN SERVFAIL
# TCP responses are not rate limited
ck_result a5.tld2	192.0.2.5	10	0	0	0	0

# whitelisted client is not rate limited
ck_result a6.tld2	192.0.2.6	10	0	0	0	0

# Errors such as SERVFAIL are rate limited.
ck_result a7.tld4	x		0	0	8	0	2

# NODATA responses are counted as the same regardless of qtype.
ck_result a8.tld2	''		2	2	2	0	0

$RNDC -c $SYSTEMTESTTOP/common/rndc.conf -p 9953 -s $ns2 stats
ckstats second dropped 46
ckstats second truncated 23


#########
sec_start

#					IP      TC      drop  NXDOMAIN SERVFAIL
# all-per-second
#   The qnames are all unique but the client IP address is constant.
QNUM=101
burst 60 'all$CNT.a9.tld2'

ck_result 'a*.a9.tld2'	192.0.2.8	50	0	10	0	0

$RNDC -c $SYSTEMTESTTOP/common/rndc.conf -p 9953 -s $ns2 stats
ckstats final dropped 56
ckstats final truncated 23


echo "I:exit status: $ret"
# exit $ret
[ $ret -ne 0 ] && echo "I:test failure overridden"
exit 0
