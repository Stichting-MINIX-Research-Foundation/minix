#!/bin/sh
#
# Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.7 2011/11/06 23:46:40 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

dotests() {
    n=`expr $n + 1`
    echo "I:test with RT, single zone ($n)"
    ret=0
    $DIG -t RT rt.rt.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with RT, two zones ($n)"
    ret=0
    $DIG -t RT rt.rt2.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with NAPTR, single zone ($n)"
    ret=0
    $DIG -t NAPTR nap.naptr.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with NAPTR, two zones ($n)"
    ret=0
    $DIG -t NAPTR nap.hang3b.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with LP ($n)"
    ret=0
    $DIG -t LP nid2.nid.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $minimal = no ] ; then
      grep "L64" dig.out.$n > /dev/null || ret=1
      grep "L32" dig.out.$n > /dev/null || ret=1
    else
      grep "L64" dig.out.$n > /dev/null && ret=1
      grep "L32" dig.out.$n > /dev/null && ret=1
    fi
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with NID ($n)"
    ret=0
    $DIG -t NID ns1.nid.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $minimal = no ] ; then
      # change && to || when we support NID additional processing
      grep "L64" dig.out.$n > /dev/null && ret=1
      grep "L32" dig.out.$n > /dev/null && ret=1
    else
      grep "L64" dig.out.$n > /dev/null && ret=1
      grep "L32" dig.out.$n > /dev/null && ret=1
    fi
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi

    n=`expr $n + 1`
    echo "I:test with NID + LP ($n)"
    ret=0
    $DIG -t NID nid2.nid.example @10.53.0.1 -p 5300 > dig.out.$n || ret=1
    if [ $minimal = no ] ; then
      # change && to || when we support NID additional processing
      grep "LP" dig.out.$n > /dev/null && ret=1
      grep "L64" dig.out.$n > /dev/null && ret=1
      grep "L32" dig.out.$n > /dev/null && ret=1
    else
      grep "LP" dig.out.$n > /dev/null && ret=1
      grep "L64" dig.out.$n > /dev/null && ret=1
      grep "L32" dig.out.$n > /dev/null && ret=1
    fi
    if [ $ret -eq 1 ] ; then
            echo "I: failed"; status=1
    fi
}

echo "I:testing with 'minimal-responses yes;'"
minimal=yes
dotests

echo "I:reconfiguring server"
cp ns1/named2.conf ns1/named.conf
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reconfig 2>&1 | sed 's/^/I:ns1 /'
sleep 2

echo "I:testing with 'minimal-responses no;'"
minimal=no
dotests

exit $status
