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

DIGOPTS="+tcp +noadd +nosea +nostat +noquest"

status=0

#
# 10.53.0.1 10.53.0.2 10.53.0.3 have a global dscp setting;
# 10.53.0.4 10.53.0.5 10.53.0.6 have dscp set in option *-source clauses;
# 10.53.0.7 has dscp set in zone *-source clauses;
#
for server in 10.53.0.1 10.53.0.2 10.53.0.3 10.53.0.4 10.53.0.5 \
	      10.53.0.6 10.53.0.7
do
	echo "I:testing root SOA lookup at $server"
	for i in 0 1 2 3 4 5 6 7 8 9
	do
		ret=0
		$DIG -p 5300 @$server $DIGOPTS soa . > dig.out.$server
		grep "status: NOERROR" dig.out.$server > /dev/null || ret=1
		test $ret = 0 && break
		sleep 1
	done
	test $ret = 0 || { echo "I:failed"; status=`expr $status + $ret`; }
done
exit $status
