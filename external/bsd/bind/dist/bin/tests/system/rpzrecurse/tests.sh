#!/bin/sh
#
# Copyright (C) 2015  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
t=0

# $1 = test name (such as 1a, 1b, etc. for which named.$1.conf exists)
run_server() {
    TESTNAME=$1

    echo "I:stopping resolver"
    $PERL $SYSTEMTESTTOP/stop.pl . ns2

    sleep 1

    echo "I:starting resolver using named.$TESTNAME.conf"
    cp -f ns2/named.$TESTNAME.conf ns2/named.conf
    $PERL $SYSTEMTESTTOP/start.pl --noclean --restart . ns2
}

run_query() {
    TESTNAME=$1
    LINE=$2

    NAME=`tail -n +"$LINE" ns2/$TESTNAME.queries | head -n 1`
    $DIG $DIGOPTS $NAME a @10.53.0.2 -p 5300 -b 127.0.0.1 > dig.out.${t}
    grep "status: SERVFAIL" dig.out.${t} > /dev/null 2>&1 && return 1
    return 0
}

# $1 = test name (such as 1a, 1b, etc. for which $1.queries exists)
# $2 = line number in query file to test (the name to query is taken from this line)
expect_norecurse() {
    TESTNAME=$1
    LINE=$2

    NAME=`tail -n +"$LINE" ns2/$TESTNAME.queries | head -n 1`
    t=`expr $t + 1`
    echo "I:testing $NAME doesn't recurse (${t})"
    run_query $TESTNAME $LINE || {
        echo "I:test ${t} failed"
        status=1
    }
}

# $1 = test name (such as 1a, 1b, etc. for which $1.queries exists)
# $2 = line number in query file to test (the name to query is taken from this line)
expect_recurse() {
    TESTNAME=$1
    LINE=$2

    NAME=`tail -n +"$LINE" ns2/$TESTNAME.queries | head -n 1`
    t=`expr $t + 1`
    echo "I:testing $NAME recurses (${t})"
    run_query $TESTNAME $LINE && {
        echo "I:test ${t} failed"
        status=1
    }
}

t=`expr $t + 1`
echo "I:testing that l1.l0 exists without RPZ (${t})"
$DIG $DIGOPTS l1.l0 ns @10.53.0.2 -p 5300 > dig.out.${t}
grep "status: NOERROR" dig.out.${t} > /dev/null 2>&1 || {
    echo "I:test ${t} failed"
    status=1
}

t=`expr $t + 1`
echo "I:testing that l2.l1.l0 returns SERVFAIL without RPZ (${t})"
$DIG $DIGOPTS l2.l1.l0 ns @10.53.0.2 -p 5300 > dig.out.${t}
grep "status: SERVFAIL" dig.out.${t} > /dev/null 2>&1 || {
    echo "I:test ${t} failed"
    status=1
}

# Group 1
run_server 1a
expect_norecurse 1a 1
run_server 1b
expect_norecurse 1b 1
expect_recurse 1b 2
run_server 1c
expect_norecurse 1c 1

# Group 2
run_server 2a
for n in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32
do
    expect_norecurse 2a $n
done
expect_recurse 2a 33

# Group 3
run_server 3a
expect_recurse 3a 1
run_server 3b
expect_recurse 3b 1
run_server 3c
expect_recurse 3c 1
run_server 3d
expect_norecurse 3d 1
expect_recurse 3d 2
run_server 3e
expect_norecurse 3e 1
expect_recurse 3e 2
run_server 3f
expect_norecurse 3f 1
expect_recurse 3f 2

# Group 4
testlist="aa ap bf"
values="1 16 32"
# Uncomment the following to test every skip value instead of 
# only a sample of values
#
#testlist="aa ab ac ad ae af ag ah ai aj ak al am an ao ap \
#          aq ar as at au av aw ax ay az ba bb bc bd be bf"
#values="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 \
#        21 22 23 24 25 26 27 28 29 30 31 32"
set -- $values
for n in $testlist; do
    run_server 4$n
    ni=$1
    t=`expr $t + 1`
    echo "I:testing that ${ni} of 33 queries skip recursion (${t})"
    c=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 \
	     17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33
    do
	run_query 4$n $i
	c=`expr $c + $?`
    done
    skipped=`expr 33 - $c`
    if [ $skipped != $ni ]; then
	echo "I:test $t failed (actual=$skipped, expected=$ni)"
	status=1
    fi
    shift
done

# Group 5
run_server 5a
expect_norecurse 5a 1
expect_norecurse 5a 2
expect_recurse 5a 3
expect_recurse 5a 4
expect_recurse 5a 5
expect_recurse 5a 6

# Group 6
echo "I:check recursive behavior consistency during policy update races"
run_server 6a
sleep 1
t=`expr $t + 1`
echo "I:running dig to cache CNAME record (${t})"
$DIG $DIGOPTS @10.53.0.2 -p 5300 www.test.example.org CNAME > dig.out.${t}
sleep 1
echo "I:suspending authority server"
kill -TSTP `cat ns1/named.pid`
echo "I:adding an NSDNAME policy"
cp ns2/db.6a.00.policy.local ns2/saved.policy.local
cp ns2/db.6b.00.policy.local ns2/db.6a.00.policy.local
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 6a.00.policy.local 2>&1 | sed 's/^/I:ns2 /'
sleep 1
t=`expr $t + 1`
echo "I:running dig to follow CNAME (blocks, so runs in the background) (${t})"
$DIG $DIGOPTS @10.53.0.2 -p 5300 www.test.example.org A > dig.out.${t} &
sleep 1
echo "I:removing the NSDNAME policy"
cp ns2/db.6c.00.policy.local ns2/db.6a.00.policy.local
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 6a.00.policy.local 2>&1 | sed 's/^/I:ns2 /'
sleep 1
echo "I:resuming authority server"
kill -CONT `cat ns1/named.pid`
for n in 1 2 3 4 5 6 7 8 9; do
    sleep 1
    [ -s dig.out.${t} ] || continue
    grep "status: NOERROR" dig.out.${t} > /dev/null 2>&1 || {
        echo "I:test ${t} failed"
        status=1
    }
done

echo "I:check recursive behavior consistency during policy removal races"
cp ns2/saved.policy.local ns2/db.6a.00.policy.local
run_server 6a
sleep 1
t=`expr $t + 1`
echo "I:running dig to cache CNAME record (${t})"
$DIG $DIGOPTS @10.53.0.2 -p 5300 www.test.example.org CNAME > dig.out.${t}
sleep 1
echo "I:suspending authority server"
kill -TSTP `cat ns1/named.pid`
echo "I:adding an NSDNAME policy"
cp ns2/db.6b.00.policy.local ns2/db.6a.00.policy.local
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 6a.00.policy.local 2>&1 | sed 's/^/I:ns2 /'
sleep 1
t=`expr $t + 1`
echo "I:running dig to follow CNAME (blocks, so runs in the background) (${t})"
$DIG $DIGOPTS @10.53.0.2 -p 5300 www.test.example.org A > dig.out.${t} &
sleep 1
echo "I:removing the policy zone"
cp ns2/named.default.conf ns2/db.6a.00.policy.local
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 6a.00.policy.local 2>&1 | sed 's/^/I:ns2 /'
sleep 1
echo "I:resuming authority server"
kill -CONT `cat ns1/named.pid`
for n in 1 2 3 4 5 6 7 8 9; do
    sleep 1
    [ -s dig.out.${t} ] || continue
    grep "status: NOERROR" dig.out.${t} > /dev/null 2>&1 || {
        echo "I:test ${t} failed"
        status=1
    }
done

# Check CLIENT-IP behavior
t=`expr $t + 1`
echo "I:testing CLIENT-IP behavior (${t})"
run_server clientip
$DIG $DIGOPTS l2.l1.l0 a @10.53.0.2 -p 5300 -b 10.53.0.4 > dig.out.${t}
grep "status: NOERROR" dig.out.${t} > /dev/null 2>&1 || {
    echo "I:test $t failed: query failed"
    status=1
}
grep "^l2.l1.l0.[[:space:]]*[0-9]*[[:space:]]*IN[[:space:]]*A[[:space:]]*10.53.0.2" dig.out.${t} > /dev/null 2>&1 || {
    echo "I:test $t failed: didn't get expected answer"
    status=1
}

exit $status
