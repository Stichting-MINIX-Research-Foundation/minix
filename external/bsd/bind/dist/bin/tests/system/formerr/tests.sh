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

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:test name to long"
$PERL formerr.pl -a 10.53.0.1 -p 5300 nametoolong > nametoolong.out
ans=`grep got: nametoolong.out`
if [ "${ans}" != "got: 000080010000000000000000" ];
then
	echo "I:failed"; status=`expr $status + 1`;
fi

echo "I:two questions"
$PERL formerr.pl -a 10.53.0.1 -p 5300 twoquestions > twoquestions.out
ans=`grep got: twoquestions.out`
if [ "${ans}" != "got: 000080010000000000000000" ];
then
	echo "I:failed"; status=`expr $status + 1`;
fi

# this one arguable could be NOERORR.
echo "I:no questions"
$PERL formerr.pl -a 10.53.0.1 -p 5300 noquestions > noquestions.out
ans=`grep got: noquestions.out`
if [ "${ans}" != "got: 000080010000000000000000" ];
then
	echo "I:failed"; status=`expr $status + 1`;
fi

echo "I:exit status: $status"

exit $status
