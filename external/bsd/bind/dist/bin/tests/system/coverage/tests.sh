#!/bin/sh
#
# Copyright (C) 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
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

COVERAGE="$COVERAGE -c ./named-compilezone"

status=0
n=1

matchall () {
    file=$1
    echo "$2" | while read matchline; do
        grep "$matchline" $file > /dev/null 2>&1 || {
            echo "FAIL"
            return
        }
    done
}

echo "I:checking for DNSSEC key coverage issues"
ret=0
for dir in [0-9][0-9]-*; do
        ret=0
        echo "I:$dir"
        args= warn= error= ok= retcode= match=
        . $dir/expect
        $COVERAGE $args -K $dir example.com > coverage.$n 2>&1

        # check that return code matches expectations
        found=$?
        if [ $found -ne $retcode ]; then
            echo "retcode was $found expected $retcode"
            ret=1
        fi

        # check for correct number of errors
        found=`grep ERROR coverage.$n | wc -l`
        if [ $found -ne $error ]; then
            echo "error count was $found expected $error"
            ret=1
        fi

        # check for correct number of warnings
        found=`grep WARNING coverage.$n | wc -l`
        if [ $found -ne $warn ]; then
            echo "warning count was $found expected $warn"
            ret=1
        fi

        # check for correct number of OKs
        found=`grep "No errors found" coverage.$n | wc -l`
        if [ $found -ne $ok ]; then
            echo "good count was $found expected $ok"
            ret=1
        fi

        found=`matchall coverage.$n "$match"`
        if [ "$found" = "FAIL" ]; then
            echo "no match on '$match'"
            ret=1
        fi

        n=`expr $n + 1`
        if [ $ret != 0 ]; then echo "I:failed"; fi
        status=`expr $status + $ret`
done

echo "I:exit status: $status"
exit $status
