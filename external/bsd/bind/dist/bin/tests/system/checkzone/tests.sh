# Copyright (C) 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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
n=1

for db in zones/good*.db
do
	echo "I:checking $db ($n)"
	ret=0
	$CHECKZONE -i local example $db > test.out.$n 2>&1 || ret=1
	n=`expr $n + 1`
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

for db in zones/bad*.db
do
	echo "I:checking $db ($n)"
	ret=0
	$CHECKZONE -i local example $db > test.out.$n 2>&1 && ret=1
	n=`expr $n + 1`
	if [ $ret != 0 ]; then echo "I:failed"; fi
	status=`expr $status + $ret`
done

echo "I:checking with journal file ($n)"
ret=0
$CHECKZONE -D -o test.orig.db test zones/test1.db > /dev/null 2>&1 || ret=1
$CHECKZONE -D -o test.changed.db test zones/test2.db > /dev/null 2>&1 || ret=1
../../makejournal test test.orig.db test.changed.db test.orig.db.jnl 2>&1 || ret=1
jlines=`$JOURNALPRINT test.orig.db.jnl | wc -l`
[ $jlines = 3 ] || ret=1
$CHECKZONE -D -j -o test.out1.db test test.orig.db > /dev/null 2>&1 || ret=1
cmp -s test.changed.db test.out1.db || ret=1
mv -f test.orig.db.jnl test.journal
$CHECKZONE -D -J test.journal -o test.out2.db test test.orig.db > /dev/null 2>&1 || ret=1
cmp -s test.changed.db test.out2.db || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking with spf warnings ($n)"
ret=0
$CHECKZONE example zones/spf.db > test.out1.$n 2>&1 || ret=1
$CHECKZONE -T ignore example zones/spf.db > test.out2.$n 2>&1 || ret=1
grep "'x.example' found type SPF" test.out1.$n > /dev/null && ret=1
grep "'y.example' found type SPF" test.out1.$n > /dev/null || ret=1
grep "'example' found type SPF" test.out1.$n > /dev/null && ret=1
grep "'x.example' found type SPF" test.out2.$n > /dev/null && ret=1
grep "'y.example' found type SPF" test.out2.$n > /dev/null && ret=1
grep "'example' found type SPF" test.out2.$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking with max ttl (text) ($n)"
ret=0
$CHECKZONE -l 300 example zones/good1.db > test.out1.$n 2>&1 && ret=1
$CHECKZONE -l 600 example zones/good1.db > test.out2.$n 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking with max ttl (raw) ($n)"
ret=0
$CHECKZONE -f raw -l 300 example good1.db.raw > test.out1.$n 2>&1 && ret=1
$CHECKZONE -f raw -l 600 example good1.db.raw > test.out2.$n 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking with max ttl (map) ($n)"
ret=0
$CHECKZONE -f map -l 300 example good1.db.map > test.out1.$n 2>&1 && ret=1
$CHECKZONE -f map -l 600 example good1.db.map > test.out2.$n 2>&1 || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for no 'inherited owner' warning on '\$INCLUDE file' with no new \$ORIGIN ($n)"
ret=0
$CHECKZONE example zones/nowarn.inherited.owner.db > test.out1.$n 2>&1 || ret=1
grep "inherited.owner" test.out1.$n > /dev/null && ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for 'inherited owner' warning on '\$ORIGIN + \$INCLUDE file' ($n)"
ret=0
$CHECKZONE example zones/warn.inherit.origin.db > test.out1.$n 2>&1 || ret=1
grep "inherited.owner" test.out1.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking for 'inherited owner' warning on '\$INCLUDE file origin' ($n)"
ret=0
$CHECKZONE example zones/warn.inherited.owner.db > test.out1.$n 2>&1 || ret=1
grep "inherited.owner" test.out1.$n > /dev/null || ret=1
n=`expr $n + 1`
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
