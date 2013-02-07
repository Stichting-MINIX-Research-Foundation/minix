#!/bin/sh
#
# Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

# $Id: tests.sh,v 1.9 2007-09-14 01:46:05 marka Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:test 2-element sortlist statement"
cat <<EOF >test1.good
a.example.		300	IN	A	192.168.3.1
a.example.		300	IN	A	192.168.1.1
a.example.		300	IN	A	1.1.1.5
a.example.		300	IN	A	1.1.1.1
a.example.		300	IN	A	1.1.1.3
a.example.		300	IN	A	1.1.1.2
a.example.		300	IN	A	1.1.1.4
EOF
$DIG +tcp +noadd +nosea +nostat +noquest +noauth +nocomm +nocmd a.example. \
	@10.53.0.1 -b 10.53.0.1 -p 5300 >test1.dig
# Note that this can't use digcomp.pl because here, the ordering of the
# result RRs is significant.
diff test1.dig test1.good || status=1

echo "I:test 1-element sortlist statement and undocumented BIND 8 features"
	cat <<EOF >test2.good
b.example.		300	IN	A	10.53.0.$n
EOF

$DIG +tcp +noadd +nosea +nostat +noquest +noauth +nocomm +nocmd \
	b.example. @10.53.0.1 -b 10.53.0.2 -p 5300 | sed 1q | \
        egrep '10.53.0.(2|3)$' > test2.out &&
$DIG +tcp +noadd +nosea +nostat +noquest +noauth +nocomm +nocmd \
	b.example. @10.53.0.1 -b 10.53.0.3 -p 5300 | sed 1q | \
        egrep '10.53.0.(2|3)$' >> test2.out &&
$DIG +tcp +noadd +nosea +nostat +noquest +noauth +nocomm +nocmd \
	b.example. @10.53.0.1 -b 10.53.0.4 -p 5300 | sed 1q | \
        egrep '10.53.0.4$' >> test2.out &&
$DIG +tcp +noadd +nosea +nostat +noquest +noauth +nocomm +nocmd \
	b.example. @10.53.0.1 -b 10.53.0.5 -p 5300 | sed 1q | \
        egrep '10.53.0.5$' >> test2.out || status=1

echo "I:exit status: $status"
exit $status
