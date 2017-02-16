#!/bin/sh
#
# Copyright (C) 2004, 2007, 2009, 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

#
echo "I: checking that we detect a NS which refers to a CNAME"
if $CHECKZONE . cname.db > cname.out 2>&1
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "is a CNAME" cname.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which is below a DNAME"
if $CHECKZONE . dname.db > dname.out 2>&1
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "is below a DNAME" dname.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which has no address records (A/AAAA)"
if $CHECKZONE . noaddress.db > noaddress.out
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "has no address records" noaddress.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which has no records"
if $CHECKZONE . nxdomain.db > nxdomain.out
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "has no address records" noaddress.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which looks like a A record (fail)"
if $CHECKZONE -n fail . a.db > a.out 2>&1
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "appears to be an address" a.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which looks like a A record (warn=default)"
if $CHECKZONE . a.db > a.out 2>&1
then
	if grep "appears to be an address" a.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
else
	echo "I:failed (status)"; status=`expr $status + 1`
fi

#
echo "I: checking that we detect a NS which looks like a A record (ignore)"
if $CHECKZONE -n ignore . a.db > a.out 2>&1
then
	if grep "appears to be an address" a.out > /dev/null
	then
		echo "I:failed (message)"; status=`expr $status + 1`
	else
		:
	fi
else
	echo "I:failed (status)"; status=`expr $status + 1`
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (fail)"
if $CHECKZONE -n fail . aaaa.db > aaaa.out 2>&1
then
	echo "I:failed (status)"; status=`expr $status + 1`
else
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (warn=default)"
if $CHECKZONE . aaaa.db > aaaa.out 2>&1
then
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=`expr $status + 1`
	fi
else
	echo "I:failed (status)"; status=`expr $status + 1`
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (ignore)"
if $CHECKZONE -n ignore . aaaa.db > aaaa.out 2>&1
then
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		echo "I:failed (message)"; status=`expr $status + 1`
	else
		:
	fi
else
	echo "I:failed (status)"; status=`expr $status + 1`
fi

#
echo "I: checking 'rdnc zonestatus' output"
ret=0 
for i in 0 1 2 3 4 5 6 7 8 9
do
	$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus master.example > rndc.out.master 2>&1
	grep "zone not loaded" rndc.out.master > /dev/null || break
	sleep 1
done
checkfor() {
	grep "$1" $2 > /dev/null || {
		ret=1;
		echo "I: missing '$1' from '$2'"
	}
}
checkfor "name: master.example" rndc.out.master
checkfor "type: master" rndc.out.master
checkfor "files: master.db, master.db.signed" rndc.out.master
checkfor "serial: " rndc.out.master
checkfor "nodes: " rndc.out.master
checkfor "last loaded: " rndc.out.master
checkfor "secure: yes" rndc.out.master
checkfor "inline signing: no" rndc.out.master
checkfor "key maintenance: automatic" rndc.out.master
checkfor "next key event: " rndc.out.master
checkfor "next resign node: " rndc.out.master
checkfor "next resign time: " rndc.out.master
checkfor "dynamic: yes" rndc.out.master
checkfor "frozen: no" rndc.out.master
for i in 0 1 2 3 4 5 6 7 8 9
do
	$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 zonestatus master.example > rndc.out.slave 2>&1
	grep "zone not loaded" rndc.out.slave > /dev/null || break
	sleep 1
done
checkfor "name: master.example" rndc.out.slave
checkfor "type: slave" rndc.out.slave
checkfor "files: slave.db" rndc.out.slave
checkfor "serial: " rndc.out.slave
checkfor "nodes: " rndc.out.slave
checkfor "next refresh: " rndc.out.slave
checkfor "expires: " rndc.out.slave
checkfor "secure: yes" rndc.out.slave
for i in 0 1 2 3 4 5 6 7 8 9
do
	$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus reload.example > rndc.out.prereload 2>&1
	grep "zone not loaded" rndc.out.prereload > /dev/null || break
	sleep 1
done
checkfor "files: reload.db, soa.db$" rndc.out.prereload
echo "@ 0 SOA . . 2 0 0 0 0" > ns1/soa.db
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload reload.example
for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG reload.example SOA @10.53.0.1 -p 5300 > dig.out
	grep " 2 0 0 0 0" dig.out >/dev/null && break
	sleep 1
done
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus reload.example > rndc.out.postreload 2>&1
checkfor "files: reload.db, soa.db$" rndc.out.postreload
sleep 1
echo "@ 0 SOA . . 3 0 0 0 0" > ns1/reload.db
echo "@ 0 NS ." >> ns1/reload.db
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 reload reload.example
for i in 0 1 2 3 4 5 6 7 8 9
do
	$DIG reload.example SOA @10.53.0.1 -p 5300 > dig.out
	grep " 3 0 0 0 0" dig.out >/dev/null && break
	sleep 1
done
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus reload.example > rndc.out.removeinclude 2>&1
checkfor "files: reload.db$" rndc.out.removeinclude

if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I: checking 'rdnc zonestatus' with duplicated zone name"
ret=0 
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus duplicate.example > rndc.out.duplicate 2>&1
checkfor "zone 'duplicate.example' was found in multiple views" rndc.out.duplicate
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus duplicate.example in primary > rndc.out.duplicate 2>&1
checkfor "name: duplicate.example" rndc.out.duplicate
$RNDC -c ../common/rndc.conf -s 10.53.0.1 -p 9953 zonestatus nosuchzone.example > rndc.out.duplicate 2>&1
checkfor "no matching zone 'nosuchzone.example' in any view" rndc.out.duplicate
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
