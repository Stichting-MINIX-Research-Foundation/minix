#!/bin/sh
#
# Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: tests.sh,v 1.22 2012/02/09 23:47:18 tbox Exp 

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=0

getsit() {
	awk '$2 == "SIT:" {
		print $3;
	}' < $1
}

havetc() {
	grep 'flags:.* tc[^;]*;' $1 > /dev/null
}

for bad in bad*.conf
do
        ret=0
        echo "I:checking that named-checkconf detects error in $bad"
        $CHECKCONF $bad > /dev/null 2>&1
        if [ $? != 1 ]; then echo "I:failed"; ret=1; fi
        status=`expr $status + $ret`
done

n=`expr $n + 1`
echo "I:checking SIT token returned to empty SIT option ($n)"
ret=0
$DIG +qr +sit version.bind txt ch @10.53.0.1 -p 5300 > dig.out.test$n
grep SIT: dig.out.test$n > /dev/null || ret=1
grep "status: NOERROR" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking response size without SIT ($n)"
ret=0
$DIG large.example txt @10.53.0.1 -p 5300 +ignore > dig.out.test$n
havetc dig.out.test$n || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking response size without valid SIT ($n)"
ret=0
$DIG +sit large.example txt @10.53.0.1 -p 5300 +ignore > dig.out.test$n
havetc dig.out.test$n || ret=1
grep "; SIT:.*(good)" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking response size with SIT ($n)"
ret=0
$DIG +sit large.example txt @10.53.0.1 -p 5300 > dig.out.test$n.l
sit=`getsit dig.out.test$n.l`
$DIG +qr +sit=$sit large.example txt @10.53.0.1 -p 5300 +ignore > dig.out.test$n
havetc dig.out.test$n && ret=1
grep "; SIT:.*(good)" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking response size with SIT recursive ($n)"
ret=0
$DIG +qr +sit=$sit large.xxx txt @10.53.0.1 -p 5300 +ignore > dig.out.test$n
havetc dig.out.test$n && ret=1
grep "; SIT:.*(good)" dig.out.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

n=`expr $n + 1`
echo "I:checking for SIT value in adb ($n)"
ret=0
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 dumpdb
sleep 1
grep "10.53.0.2.*\[sit=" ns1/named_dump.db > /dev/null|| ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
