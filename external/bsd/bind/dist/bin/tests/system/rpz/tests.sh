# Copyright (C) 2011-2015  Internet Systems Consortium, Inc. ("ISC")
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

# Id


# test response policy zones (RPZ)

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

ns=10.53.0
ns1=$ns.1		# root, defining the others
ns2=$ns.2		# authoritative server whose records are rewritten
ns3=$ns.3		# main rewriting resolver
ns4=$ns.4		# another authoritative server that is rewritten
ns5=$ns.5		# another rewriting resolver
ns6=$ns.6		# a forwarding server
ns7=$ns.7		# another rewriting resolver

HAVE_CORE=
SAVE_RESULTS=


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

TS='%H:%M:%S '
TS=
comment () {
    if test -n "$TS"; then
	date "+I:${TS}$*"
    fi
}

RNDCCMD="$RNDC -c $SYSTEMTESTTOP/common/rndc.conf -p 9953 -s"

digcmd () {
    if test "$1" = TCP; then
	shift
    fi
    # Default to +noauth and @$ns3
    # Also default to -bX where X is the @value so that OS X will choose
    #	    the right IP source address.
    digcmd_args=`echo "+noadd +time=2 +tries=1 -p 5300 $*" |	\
	    sed -e "/@/!s/.*/& @$ns3/"				\
		-e '/-b/!s/@\([^ ]*\)/@\1 -b\1/'		\
		-e '/+n?o?auth/!s/.*/+noauth &/'`
    #echo I:dig $digcmd_args 1>&2
    $DIG $digcmd_args
}

# set DIGNM=file name for dig output
GROUP_NM=
TEST_NUM=0
make_dignm () {
    TEST_NUM=`expr $TEST_NUM + 1`
    DIGNM=dig.out$GROUP_NM-$TEST_NUM
    while test -f $DIGNM; do
	TEST_NUM="$TEST_NUM+"
	DIGNM=dig.out$GROUP_NM-$TEST_NUM
    done
}

setret () {
    ret=1
    status=`expr $status + 1`
    echo "$*"
}

# (re)load the reponse policy zones with the rules in the file $TEST_FILE
load_db () {
    if test -n "$TEST_FILE"; then
	if $NSUPDATE -v $TEST_FILE; then :
	    $RNDCCMD $ns3 sync
	else
	    echo "I:failed to update policy zone with $TEST_FILE"
	    $RNDCCMD $ns3 sync
	    exit 1
	fi
    fi
}

restart () {
    # try to ensure that the server really has stopped
    # and won't mess with ns$1/name.pid
    if test -z "$HAVE_CORE" -a -f ns$1/named.pid; then
	$RNDCCMD $ns$1 halt >/dev/null 2>&1
	if test -f ns$1/named.pid; then
	    sleep 1
	    PID=`cat ns$1/named.pid 2>/dev/null`
	    if test -n "$PID"; then
		echo "I:killing ns$1 server $PID"
		kill -9 $PID
	    fi
	fi
    fi
    rm -f ns$1/*.jnl
    if test -f ns$1/base.db; then
	for NM in ns$1/bl*.db; do
	    cp -f ns$1/base.db $NM
	done
    fi
    $PERL $SYSTEMTESTTOP/start.pl --noclean --restart . ns$1
    load_db
}

# $1=server and irrelevant args  $2=error message
ckalive () {
    CKALIVE_NS=`expr "$1" : '.*@ns\([1-9]\).*'`
    if test -z "$CKALIVE_NS"; then
	CKALIVE_NS=3
    fi
    eval CKALIVE_IP=\$ns$CKALIVE_NS
    $RNDCCMD $CKALIVE_IP status >/dev/null 2>&1 && return 0
    HAVE_CORE=yes
    setret "$2"
    # restart the server to avoid stalling waiting for it to stop
    restart $CKALIVE_NS
    return 1
}

ckstats () {
    HOST=$1
    LABEL="$2"
    NSDIR="$3"
    EXPECTED="$4"
    $RNDCCMD $HOST stats
    NEW_CNT=0`sed -n -e 's/[	 ]*\([0-9]*\).response policy.*/\1/p'  \
		    $NSDIR/named.stats | tail -1`
    eval "OLD_CNT=0\$${NSDIR}_CNT"
    GOT=`expr $NEW_CNT - $OLD_CNT`
    if test "$GOT" -ne "$EXPECTED"; then
	setret "I:wrong $LABEL $NSDIR statistics of $GOT instead of $EXPECTED"
    fi
    eval "${NSDIR}_CNT=$NEW_CNT"
}

ckstatsrange () {
    HOST=$1
    LABEL="$2"
    NSDIR="$3"
    MIN="$4"
    MAX="$5"
    $RNDCCMD $HOST stats
    NEW_CNT=0`sed -n -e 's/[	 ]*\([0-9]*\).response policy.*/\1/p'  \
		    $NSDIR/named.stats | tail -1`
    eval "OLD_CNT=0\$${NSDIR}_CNT"
    GOT=`expr $NEW_CNT - $OLD_CNT`
    if test "$GOT" -lt "$MIN" -o "$GOT" -gt "$MAX"; then
	setret "I:wrong $LABEL $NSDIR statistics of $GOT instead of ${MIN}..${MAX}"
    fi
    eval "${NSDIR}_CNT=$NEW_CNT"
}

# $1=message  $2=optional test file name
start_group () {
    ret=0
    test -n "$1" && date "+I:${TS}checking $1"
    TEST_FILE=$2
    if test -n "$TEST_FILE"; then
	GROUP_NM="-$TEST_FILE"
	load_db
    else
	GROUP_NM=
    fi
    TEST_NUM=0
}

end_group () {
    if test -n "$TEST_FILE"; then
	# remove the previous set of test rules
	sed -e 's/[	 ]add[	 ]/ delete /' $TEST_FILE | $NSUPDATE
	TEST_FILE=
    fi
    ckalive $ns3 "I:failed; ns3 server crashed and restarted"
    GROUP_NM=
}

clean_result () {
    if test -z "$SAVE_RESULTS"; then
	rm -f $*
    fi
}

# $1=dig args $2=other dig output file
ckresult () {
    #ckalive "$1" "I:server crashed by 'dig $1'" || return 1
    if grep "flags:.* aa .*ad;" $DIGNM; then
	setret "I:'dig $1' AA and AD set;"
    elif grep "flags:.* aa .*ad;" $DIGNM; then
	setret "I:'dig $1' AD set;"
    fi
    if $PERL $SYSTEMTESTTOP/digcomp.pl $DIGNM $2 >/dev/null; then
	NEED_TCP=`echo "$1" | sed -n -e 's/[Tt][Cc][Pp].*/TCP/p'`
	RESULT_TCP=`sed -n -e 's/.*Truncated, retrying in TCP.*/TCP/p' $DIGNM`
	if test "$NEED_TCP" != "$RESULT_TCP"; then
	    setret "I:'dig $1' wrong; no or unexpected truncation in $DIGNM"
	    return 1
	fi
	clean_result ${DIGNM}*
	return 0
    fi
    setret "I:'dig $1' wrong; diff $DIGNM $2"
    return 1
}

# check only that the server does not crash
# $1=target domain  $2=optional query type
nocrash () {
    digcmd $* >/dev/null
    ckalive "$*" "I:server crashed by 'dig $*'"
}


# check rewrite to NXDOMAIN
# $1=target domain  $2=optional query type
nxdomain () {
    make_dignm
    digcmd $*								\
	| sed -e 's/^[a-z].*	IN	CNAME	/;xxx &/'		\
		-e 's/^[a-z].*	IN	RRSIG	/;xxx &/'		\
	    >$DIGNM
    ckresult "$*" proto.nxdomain
}

# check rewrite to NODATA
# $1=target domain  $2=optional query type
nodata () {
    make_dignm
    digcmd $*								\
	| sed -e 's/^[a-z].*	IN	CNAME	/;xxx &/' >$DIGNM
    ckresult "$*" proto.nodata
}

# check rewrite to an address
#   modify the output so that it is easily compared, but save the original line
# $1=IPv4 address  $2=digcmd args  $3=optional TTL
addr () {
    ADDR=$1
    make_dignm
    digcmd $2 >$DIGNM
    #ckalive "$2" "I:server crashed by 'dig $2'" || return 1
    ADDR_ESC=`echo "$ADDR" | sed -e 's/\./\\\\./g'`
    ADDR_TTL=`sed -n -e "s/^[-.a-z0-9]\{1,\}	*\([0-9]*\)	IN	AA*	${ADDR_ESC}\$/\1/p" $DIGNM`
    if test -z "$ADDR_TTL"; then
	setret "I:'dig $2' wrong; no address $ADDR record in $DIGNM"
	return 1
    fi
    if test -n "$3" && test "$ADDR_TTL" -ne "$3"; then
	setret "I:'dig $2' wrong; TTL=$ADDR_TTL instead of $3 in $DIGNM"
	return 1
    fi
    clean_result ${DIGNM}*
}

# Check that a response is not rewritten
#   Use $ns1 instead of the authority for most test domains, $ns2 to prevent
#   spurious differences for `dig +norecurse`
# $1=optional "TCP"  remaining args for dig
nochange () {
    make_dignm
    digcmd $* >$DIGNM
    digcmd $* @$ns1 >${DIGNM}_OK
    ckresult "$*" ${DIGNM}_OK && clean_result ${DIGNM}_OK
}

# check against a 'here document'
here () {
    make_dignm
    sed -e 's/^[	 ]*//' >${DIGNM}_OK
    digcmd $* >$DIGNM
    ckresult "$*" ${DIGNM}_OK
}

# check dropped response
DROPPED='^;; connection timed out; no servers could be reached'
drop () {
    make_dignm
    digcmd $* >$DIGNM
    if grep "$DROPPED" $DIGNM >/dev/null; then
	clean_result ${DIGNM}*
	return 0
    fi
    setret "I:'dig $1' wrong; response in $DIGNM"
    return 1
}


# make prototype files to check against rewritten results
digcmd nonexistent @$ns2 >proto.nxdomain
digcmd txt-only.tld2 @$ns2 >proto.nodata


status=0

start_group "QNAME rewrites" test1
nochange .				# 1 do not crash or rewrite root
nxdomain a0-1.tld2			# 2
nodata a3-1.tld2			# 3
nodata a3-2.tld2			# 4 nodata at DNAME itself
nochange sub.a3-2.tld2			# 5 miss where DNAME might work
nxdomain a4-2.tld2			# 6 rewrite based on CNAME target
nxdomain a4-2-cname.tld2		# 7
nodata a4-3-cname.tld2			# 8
addr 12.12.12.12  a4-1.sub1.tld2	# 9 A replacement
addr 12.12.12.12  a4-1.sub2.tld2	# 10 A replacement with wildcard
addr 12.12.12.12  nxc1.sub1.tld2	# 11 replace NXDOMAIN with CNAME
addr 12.12.12.12  nxc2.sub1.tld2	# 12 replace NXDOMAIN with CNAME chain
addr 127.4.4.1	  a4-4.tld2		# 13 prefer 1st conflicting QNAME zone
nochange a6-1.tld2			# 14
addr 127.6.2.1	  a6-2.tld2		# 15
addr 56.56.56.56  a3-6.tld2		# 16 wildcard CNAME
addr 57.57.57.57  a3-7.sub1.tld2	# 17 wildcard CNAME
addr 127.0.0.16	  a4-5-cname3.tld2	# 18 CNAME chain
addr 127.0.0.17	  a4-6-cname3.tld2	# 19 stop short in CNAME chain
nochange a5-2.tld2	    +norecurse	# 20 check that RD=1 is required
nochange a5-3.tld2	    +norecurse	# 21
nochange a5-4.tld2	    +norecurse	# 22
nochange sub.a5-4.tld2	    +norecurse	# 23
nxdomain c1.crash2.tld3			# 24 assert in rbtdb.c
nxdomain a0-1.tld2	    +dnssec	# 25 simple DO=1 without signatures
nxdomain a0-1.tld2s	    +nodnssec	# 26 simple DO=0 with signatures
nochange a0-1.tld2s	    +dnssec	# 27 simple DO=1 with signatures
nxdomain a0-1s-cname.tld2s  +dnssec	# 28 DNSSEC too early in CNAME chain
nochange a0-1-scname.tld2   +dnssec	# 29 DNSSEC on target in CNAME chain
nochange a0-1.tld2s srv +auth +dnssec	# 30 no write for DNSSEC and no record
nxdomain a0-1.tld2s srv	    +nodnssec	# 31
drop a3-8.tld2 any			# 32 drop
nochange tcp a3-9.tld2			# 33 tcp-only
here x.servfail <<'EOF'			# 34 qname-wait-recurse yes
    ;; status: SERVFAIL, x
EOF
addr 35.35.35.35 "x.servfail @$ns5"	# 35 qname-wait-recurse no
end_group
ckstats $ns3 test1 ns3 22
ckstats $ns5 test1 ns5 1
ckstats $ns6 test1 ns6 0

start_group "NXDOMAIN/NODATA action on QNAME trigger" test1
nxdomain a0-1.tld2 @$ns6                   # 1
nodata a3-1.tld2 @$ns6                     # 2
nodata a3-2.tld2 @$ns6                     # 3 nodata at DNAME itself
nxdomain a4-2.tld2 @$ns6                   # 4 rewrite based on CNAME target
nxdomain a4-2-cname.tld2 @$ns6             # 5
nodata a4-3-cname.tld2 @$ns6               # 6
addr 12.12.12.12  "a4-1.sub1.tld2 @$ns6"   # 7 A replacement
addr 12.12.12.12  "a4-1.sub2.tld2 @$ns6"   # 8 A replacement with wildcard
addr 127.4.4.1    "a4-4.tld2 @$ns6"        # 9 prefer 1st conflicting QNAME zone
addr 12.12.12.12  "nxc1.sub1.tld2 @$ns6"   # 10 replace NXDOMAIN w/ CNAME
addr 12.12.12.12  "nxc2.sub1.tld2 @$ns6"   # 11 replace NXDOMAIN w/ CNAME chain
addr 127.6.2.1    "a6-2.tld2 @$ns6"        # 12
addr 56.56.56.56  "a3-6.tld2 @$ns6"        # 13 wildcard CNAME
addr 57.57.57.57  "a3-7.sub1.tld2 @$ns6"   # 14 wildcard CNAME
addr 127.0.0.16   "a4-5-cname3.tld2 @$ns6" # 15 CNAME chain
addr 127.0.0.17   "a4-6-cname3.tld2 @$ns6" # 16 stop short in CNAME chain
nxdomain c1.crash2.tld3 @$ns6              # 17 assert in rbtdb.c
nxdomain a0-1.tld2 +dnssec @$ns6           # 18 simple DO=1 without sigs
nxdomain a0-1s-cname.tld2s  +dnssec @$ns6  # 19
drop a3-8.tld2 any @$ns6                   # 20 drop

end_group
ckstatsrange $ns3 test1 ns3 22 25
ckstats $ns5 test1 ns5 0
ckstats $ns6 test1 ns6 0

start_group "IP rewrites" test2
nodata a3-1.tld2			# 1 NODATA
nochange a3-2.tld2			# 2 no policy record so no change
nochange a4-1.tld2			# 3 obsolete PASSTHRU record style
nxdomain a4-2.tld2			# 4
nochange a4-2.tld2 -taaaa		# 5 no A => no policy rewrite
nochange a4-2.tld2 -ttxt		# 6 no A => no policy rewrite
nxdomain a4-2.tld2 -tany		# 7 no A => no policy rewrite
nodata a4-3.tld2			# 8
nxdomain a3-1.tld2 -taaaa		# 9 IPv6 policy
nochange a4-1-aaaa.tld2 -taaaa		# 10
addr 127.0.0.1	 a5-1-2.tld2		# 11 prefer smallest policy address
addr 127.0.0.1	 a5-3.tld2		# 12 prefer first conflicting IP zone
nochange a5-4.tld2	    +norecurse	# 13 check that RD=1 is required for #14
addr 14.14.14.14 a5-4.tld2		# 14 prefer QNAME to IP
nochange a4-4.tld2			# 15 PASSTHRU
nxdomain c2.crash2.tld3			# 16 assert in rbtdb.c
addr 127.0.0.17 "a4-4.tld2 -b $ns1"	# 17 client-IP address trigger
nxdomain a7-1.tld2			# 18 slave policy zone (RT34450)
cp ns2/blv2.tld2.db.in ns2/bl.tld2.db
$RNDCCMD 10.53.0.2 reload bl.tld2
goodsoa="rpz.tld2. hostmaster.ns.tld2. 2 3600 1200 604800 60"
for i in 0 1 2 3 4 5 6 7 8 9 10
do
	soa=`$DIG -p 5300 +short soa bl.tld2 @10.53.0.3 -b10.53.0.3`
	test "$soa" = "$goodsoa" && break
	sleep 1
done
nochange a7-1.tld2			# 19 PASSTHRU
sleep 1	# ensure that a clock tick has occured so that the reload takes effect
cp ns2/blv3.tld2.db.in ns2/bl.tld2.db
goodsoa="rpz.tld2. hostmaster.ns.tld2. 3 3600 1200 604800 60"
$RNDCCMD 10.53.0.2 reload bl.tld2
for i in 0 1 2 3 4 5 6 7 8 9 10
do
	soa=`$DIG -p 5300 +short soa bl.tld2 @10.53.0.3 -b10.53.0.3`
	test "$soa" = "$goodsoa" && break
	sleep 1
done
nxdomain a7-1.tld2			# 20 slave policy zone (RT34450)
end_group
ckstats $ns3 test2 ns3 12

# check that IP addresses for previous group were deleted from the radix tree
start_group "radix tree deletions"
nochange a3-1.tld2
nochange a3-2.tld2
nochange a4-1.tld2
nochange a4-2.tld2
nochange a4-2.tld2 -taaaa
nochange a4-2.tld2 -ttxt
nochange a4-2.tld2 -tany
nochange a4-3.tld2
nochange a3-1.tld2 -tAAAA
nochange a4-1-aaaa.tld2 -tAAAA
nochange a5-1-2.tld2
end_group
ckstats $ns3 'radix tree deletions' ns3 0

if ./rpz nsdname; then
    # these tests assume "min-ns-dots 0"
    start_group "NSDNAME rewrites" test3
    nochange a3-1.tld2			# 1
    nochange a3-1.tld2	    +dnssec	# 2 this once caused problems
    nxdomain a3-1.sub1.tld2		# 3 NXDOMAIN *.sub1.tld2 by NSDNAME
    nxdomain a3-1.subsub.sub1.tld2
    nxdomain a3-1.subsub.sub1.tld2 -tany
    addr 12.12.12.12 a4-2.subsub.sub2.tld2 # 6 walled garden for *.sub2.tld2
    nochange a3-2.tld2.			# 7 exempt rewrite by name
    nochange a0-1.tld2.			# 8 exempt rewrite by address block
    addr 12.12.12.12 a4-1.tld2		# 9 prefer QNAME policy to NSDNAME
    addr 127.0.0.1 a3-1.sub3.tld2	# 10 prefer policy for largest NSDNAME
    addr 127.0.0.2 a3-1.subsub.sub3.tld2
    nxdomain xxx.crash1.tld2		# 12 dns_db_detachnode() crash
    end_group
    ckstats $ns3 test3 ns3 7
else
    echo "I:NSDNAME not checked; named configured with --disable-rpz-nsdname"
fi

if ./rpz nsip; then
    # these tests assume "min-ns-dots 0"
    start_group "NSIP rewrites" test4
    nxdomain a3-1.tld2			# 1 NXDOMAIN for all of tld2
    nochange a3-2.tld2.			# 2 exempt rewrite by name
    nochange a0-1.tld2.			# 3 exempt rewrite by address block
    nochange a3-1.tld4			# 4 different NS IP address
    end_group

    start_group "walled garden NSIP rewrites" test4a
    addr 41.41.41.41 a3-1.tld2		# 1 walled garden for all of tld2
    addr 2041::41   'a3-1.tld2 AAAA'	# 2 walled garden for all of tld2
    here a3-1.tld2 TXT <<'EOF'		# 3 text message for all of tld2
    ;; status: NOERROR, x
    a3-1.tld2.	    x	IN	TXT   "NSIP walled garden"
EOF
    end_group
    ckstats $ns3 test4 ns3 4
else
    echo "I:NSIP not checked; named configured with --disable-rpz-nsip"
fi

# policies in ./test5 overridden by response-policy{} in ns3/named.conf
#   and in ns5/named.conf
start_group "policy overrides" test5
addr 127.0.0.1 a3-1.tld2		# 1 bl-given
nochange a3-2.tld2			# 2 bl-passthru
nochange a3-3.tld2			# 3 bl-no-op	obsolete for passthru
nochange a3-4.tld2			# 4 bl-disabled
nodata a3-5.tld2			# 5 bl-nodata	zone recursive-only no
nodata a3-5.tld2    +norecurse		# 6 bl-nodata	zone recursive-only no
nodata a3-5.tld2			# 7 bl-nodata		not needed
nxdomain a3-5.tld2  +norecurse	@$ns5	# 8 bl-nodata	global recursive-only no
nxdomain a3-5.tld2s		@$ns5	# 9 bl-nodata	global break-dnssec
nxdomain a3-5.tld2s +dnssec	@$ns5	# 10 bl-nodata	global break-dnssec
nxdomain a3-6.tld2			# 11 bl-nxdomain
here a3-7.tld2 -tany <<'EOF'
    ;; status: NOERROR, x
    a3-7.tld2.	    x	IN	CNAME   txt-only.tld2.
    txt-only.tld2.  x	IN	TXT     "txt-only-tld2"
EOF
addr 58.58.58.58 a3-8.tld2		# 13 bl_wildcname
addr 59.59.59.59 a3-9.sub9.tld2		# 14 bl_wildcname
addr 12.12.12.12 a3-15.tld2		# 15 bl-garden	via CNAME to a12.tld2
addr 127.0.0.16 a3-16.tld2	    100	# 16 bl		    max-policy-ttl 100
addr 17.17.17.17 "a3-17.tld2 @$ns5" 90	# 17 ns5 bl	    max-policy-ttl 90
drop a3-18.tld2 any			# 18 bl-drop
nxdomain TCP a3-19.tld2			# 19 bl-tcp-only
end_group
ckstats $ns3 test5 ns3 12
ckstats $ns5 test5 ns5 4


# check that miscellaneous bugs are still absent
start_group "crashes" test6
for Q in RRSIG SIG ANY 'ANY +dnssec'; do
    nocrash a3-1.tld2 -t$Q
    nocrash a3-2.tld2 -t$Q
    nocrash a3-5.tld2 -t$Q
    nocrash www.redirect -t$Q
    nocrash www.credirect -t$Q
done

# This is not a bug, because any data leaked by writing 24.4.3.2.10.rpz-ip
# (or whatever) is available by publishing "foo A 10.2.3.4" and then
# resolving foo.
# nxdomain 32.3.2.1.127.rpz-ip
end_group
ckstats $ns3 bugs ns3 8



# superficial test for major performance bugs
QPERF=`sh qperf.sh`
if test -n "$QPERF"; then
    perf () {
	date "+I:${TS}checking performance $1"
	# Dry run to prime everything
	comment "before dry run $1"
	$QPERF -c -1 -l30 -d ns5/requests -s $ns5 -p 5300 >/dev/null
	comment "before real test $1"
	PFILE="ns5/$2.perf"
	$RNDCCMD $ns5 notrace
	$QPERF -c -1 -l30 -d ns5/requests -s $ns5 -p 5300 >$PFILE
	comment "after test $1"
	X=`sed -n -e 's/.*Returned *\([^ ]*:\) *\([0-9]*\) .*/\1\2/p' $PFILE \
		| tr '\n' ' '`
	if test "$X" != "$3"; then
	    setret "I:wrong results '$X' in $PFILE"
	fi
	ckalive $ns5 "I:failed; server #5 crashed"
    }
    trim () {
	sed -n -e 's/.*Queries per second: *\([0-9]*\).*/\1/p' ns5/$1.perf
    }

    # get qps with rpz
    perf 'with RPZ' rpz 'NOERROR:2900 NXDOMAIN:100 '
    RPZ=`trim rpz`

    # turn off rpz and measure qps again
    echo "# RPZ off" >ns5/rpz-switch
    RNDCCMD_OUT=`$RNDCCMD $ns5 reload`
    perf 'without RPZ' norpz 'NOERROR:3000 '
    NORPZ=`trim norpz`

    PERCENT=`expr \( "$RPZ" \* 100 + \( $NORPZ / 2 \) \) / $NORPZ`
    echo "I:$RPZ qps with RPZ is $PERCENT% of $NORPZ qps without RPZ"

    MIN_PERCENT=30
    if test "$PERCENT" -lt $MIN_PERCENT; then
	echo "I:$RPZ qps with rpz or $PERCENT% is below $MIN_PERCENT% of $NORPZ qps"
    fi

    if test "$PERCENT" -ge 100; then
	echo "I:$RPZ qps with RPZ or $PERCENT% of $NORPZ qps without RPZ is too high"
    fi

    ckstats $ns5 performance ns5 200

else
    echo "I:performance not checked; queryperf not available"
fi


# restart the main test RPZ server to see if that creates a core file
if test -z "$HAVE_CORE"; then
    $PERL $SYSTEMTESTTOP/stop.pl . ns3
    restart 3
    HAVE_CORE=`find ns* -name '*core*' -print`
    test -z "$HAVE_CORE" || setret "I:found $HAVE_CORE; memory leak?"
fi

# look for complaints from lib/dns/rpz.c and bin/name/query.c
EMSGS=`egrep -l 'invalid rpz|rpz.*failed' ns*/named.run`
if test -n "$EMSGS"; then
    setret "I:error messages in $EMSGS starting with:"
    egrep 'invalid rpz|rpz.*failed' ns*/named.run | sed -e '10,$d' -e 's/^/I:  /'
fi

echo "I:checking that ttl values are not zeroed when qtype is '*'"
$DIG +noall +answer -p 5300 @$ns3 any a3-2.tld2 > dig.out.any
ttl=`awk '/a3-2 tld2 text/ {print $2}' dig.out.any`
if test ${ttl:=0} -eq 0; then setret I:failed; fi

echo "I:checking rpz updates/transfers with parent nodes added after children"
# regression test for RT #36272: the success condition
# is the slave server not crashing.
nsd() {
    $NSUPDATE -p 5300 << EOF
server $1
ttl 300
update $2 $3 IN CNAME .
update $2 $4 IN CNAME .
send
EOF
    sleep 2
}

for i in 1 2 3 4 5; do
    nsd $ns5 add example.com.policy1. '*.example.com.policy1.'
    nsd $ns5 delete example.com.policy1. '*.example.com.policy1.'
done
for i in 1 2 3 4 5; do
    nsd $ns5 add '*.example.com.policy1.' example.com.policy1.
    nsd $ns5 delete '*.example.com.policy1.' example.com.policy1.
done

echo "I:checking that going from a empty policy zone works"
nsd $ns5 add '*.x.servfail.policy2.' x.servfail.policy2.
sleep 1
$RNDCCMD $ns7 reload policy2
$DIG z.x.servfail -p 5300 @$ns7 > dig.out.ns7
grep NXDOMAIN dig.out.ns7 > /dev/null || setret I:failed;

echo "I:exit status: $status"
exit $status
