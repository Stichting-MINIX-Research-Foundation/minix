#!/bin/sh
#
# Copyright (C) 2006-2008  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.8 2008-10-09 21:27:52 each Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

if grep "^#define DNS_RDATASET_FIXED" $TOP/config.h > /dev/null 2>&1 ; then
        test_fixed=true
else
        echo "I: Order 'fixed' disabled at compile time"
        test_fixed=false
fi

#
#
#
if $test_fixed; then
    echo "I: Checking order fixed (master)"
    ret=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
    do
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.1 fixed.example > dig.out.fixed || ret=1
    cmp -s dig.out.fixed dig.out.fixed.good || ret=1
    done
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

#
#
#
echo "I: Checking order cyclic (master)"
ret=0
matches=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    j=`expr $i % 4`
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.1 cyclic.example > dig.out.cyclic || ret=1
    if [ $i -le 4 ]; then
        cp dig.out.cyclic dig.out.$j
    else
        cmp -s dig.out.cyclic dig.out.$j && matches=`expr $matches + 1`
    fi
done
cmp -s dig.out.0 dig.out.1 && ret=1
cmp -s dig.out.0 dig.out.2 && ret=1
cmp -s dig.out.0 dig.out.3 && ret=1
cmp -s dig.out.1 dig.out.2 && ret=1
cmp -s dig.out.1 dig.out.3 && ret=1
cmp -s dig.out.2 dig.out.3 && ret=1
if [ $matches -ne 16 ]; then ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: Checking order random (master)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
	eval match$i=0
done
for i in a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 9
do
$DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	-p 5300 @10.53.0.1 random.example > dig.out.random || ret=1
	match=0
	for j in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
	do
		eval "cmp -s dig.out.random dig.out.random.good$j && match$j=1 match=1"
		if [ $match -eq 1 ]; then break; fi
	done
	if [ $match -eq 0 ]; then ret=1; fi
done
match=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
	eval "match=\`expr \$match + \$match$i\`"
done
echo "I: Random selection return $match of 24 possible orders in 36 samples"
if [ $match -lt 8 ]; then echo ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

#
#
#
if $test_fixed; then
    echo "I: Checking order fixed (slave)"
    ret=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
    do
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.2 fixed.example > dig.out.fixed || ret=1
    cmp -s dig.out.fixed dig.out.fixed.good || ret=1
    done
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

#
#
#
echo "I: Checking order cyclic (slave)"
ret=0
matches=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    j=`expr $i % 4`
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.2 cyclic.example > dig.out.cyclic || ret=1
    if [ $i -le 4 ]; then
        cp dig.out.cyclic dig.out.$j
    else
        cmp -s dig.out.cyclic dig.out.$j && matches=`expr $matches + 1`
    fi
done
cmp -s dig.out.0 dig.out.1 && ret=1
cmp -s dig.out.0 dig.out.2 && ret=1
cmp -s dig.out.0 dig.out.3 && ret=1
cmp -s dig.out.1 dig.out.2 && ret=1
cmp -s dig.out.1 dig.out.3 && ret=1
cmp -s dig.out.2 dig.out.3 && ret=1
if [ $matches -ne 16 ]; then ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: Checking order random (slave)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
	eval match$i=0
done
for i in a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 9
do
$DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	-p 5300 @10.53.0.2 random.example > dig.out.random || ret=1
	match=0
	for j in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
	do
		eval "cmp -s dig.out.random dig.out.random.good$j && match$j=1 match=1"
		if [ $match -eq 1 ]; then break; fi
	done
	if [ $match -eq 0 ]; then ret=1; fi
done
match=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
eval "match=\`expr \$match + \$match$i\`"
done
echo "I: Random selection return $match of 24 possible orders in 36 samples"
if [ $match -lt 8 ]; then echo ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: Shutting down slave"

(cd ..; sh stop.sh rrsetorder ns2 )

echo "I: Checking for slave's on disk copy of zone"

if [ ! -f ns2/root.bk ]
then
	echo "I:failed";
	status=`expr $status + 1`
fi

echo "I: Re-starting slave"

(cd ..; sh start.sh --noclean rrsetorder ns2 )

#
#
#
if $test_fixed; then
    echo "I: Checking order fixed (slave loaded from disk)"
    ret=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
    do
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.2 fixed.example > dig.out.fixed || ret=1
    cmp -s dig.out.fixed dig.out.fixed.good || ret=1
    done
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

#
#
#
echo "I: Checking order cyclic (slave loaded from disk)"
ret=0
matches=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    j=`expr $i % 4`
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.2 cyclic.example > dig.out.cyclic || ret=1
    if [ $i -le 4 ]; then
        cp dig.out.cyclic dig.out.$j
    else
        cmp -s dig.out.cyclic dig.out.$j && matches=`expr $matches + 1`
    fi
done
cmp -s dig.out.0 dig.out.1 && ret=1
cmp -s dig.out.0 dig.out.2 && ret=1
cmp -s dig.out.0 dig.out.3 && ret=1
cmp -s dig.out.1 dig.out.2 && ret=1
cmp -s dig.out.1 dig.out.3 && ret=1
cmp -s dig.out.2 dig.out.3 && ret=1
if [ $matches -ne 16 ]; then ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: Checking order random (slave loaded from disk)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
	eval match$i=0
done
for i in a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 9
do
$DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	-p 5300 @10.53.0.2 random.example > dig.out.random || ret=1
	match=0
	for j in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
	do
		eval "cmp -s dig.out.random dig.out.random.good$j && match$j=1 match=1"
		if [ $match -eq 1 ]; then break; fi
	done
	if [ $match -eq 0 ]; then ret=1; fi
done
match=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
eval "match=\`expr \$match + \$match$i\`"
done
echo "I: Random selection return $match of 24 possible orders in 36 samples"
if [ $match -lt 8 ]; then echo ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

#
#
#
if $test_fixed; then
    echo "I: Checking order fixed (cache)"
    ret=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
    do
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.3 fixed.example > dig.out.fixed || ret=1
    cmp -s dig.out.fixed dig.out.fixed.good || ret=1
    done
    if [ $ret != 0 ]; then echo "I:failed"; fi
    status=`expr $status + $ret`
fi

#
#
#
echo "I: Checking order cyclic (cache)"
ret=0
matches=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    j=`expr $i % 4`
    $DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	    -p 5300 @10.53.0.3 cyclic.example > dig.out.cyclic || ret=1
    if [ $i -le 4 ]; then
        cp dig.out.cyclic dig.out.$j
    else
        cmp -s dig.out.cyclic dig.out.$j && matches=`expr $matches + 1`
    fi
done
cmp -s dig.out.0 dig.out.1 && ret=1
cmp -s dig.out.0 dig.out.2 && ret=1
cmp -s dig.out.0 dig.out.3 && ret=1
cmp -s dig.out.1 dig.out.2 && ret=1
cmp -s dig.out.1 dig.out.3 && ret=1
cmp -s dig.out.2 dig.out.3 && ret=1
if [ $matches -ne 16 ]; then ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: Checking order random (cache)"
ret=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
	eval match$i=0
done
for i in a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 9
do
$DIG +nosea +nocomm +nocmd +noquest +noadd +noauth +nocomm +nostat +short \
	-p 5300 @10.53.0.3 random.example > dig.out.random || ret=1
	match=0
	for j in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
	do
		eval "cmp -s dig.out.random dig.out.random.good$j && match$j=1 match=1"
		if [ $match -eq 1 ]; then break; fi
	done
	if [ $match -eq 0 ]; then ret=1; fi
done
match=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
eval "match=\`expr \$match + \$match$i\`"
done
echo "I: Random selection return $match of 24 possible orders in 36 samples"
if [ $match -lt 8 ]; then echo ret=1; fi
if [ $ret != 0 ]; then echo "I:failed"; fi

status=`expr $status + $ret`
echo "I:exit status: $status"
exit $status
