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
echo "I:check lookups against zero TTL records"
i=0
passes=10
$DIG -p 5300 @10.53.0.2 axfr example | 
awk '$2 == "0" { print "-q", $1, $4; print "-q", "zzz"$1, $4;}' > query.list
while [ $i -lt $passes ]
do
	ret=0
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.1 &
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.2 &
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.3 &
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.4 &
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.5 &
	$DIG -p 5300 @10.53.0.3 -f query.list > dig.out$i.6 &
	wait
	grep "status: SERVFAIL" dig.out$i.1 && ret=1
	grep "status: SERVFAIL" dig.out$i.2 && ret=1
	grep "status: SERVFAIL" dig.out$i.3 && ret=1
	grep "status: SERVFAIL" dig.out$i.5 && ret=1
	grep "status: SERVFAIL" dig.out$i.6 && ret=1
	grep "status: SERVFAIL" dig.out$i.6 && ret=1
	[ $ret = 1 ] && break
	i=`expr $i + 1`
	echo "I: successfully completed pass $i of $passes"
done
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
