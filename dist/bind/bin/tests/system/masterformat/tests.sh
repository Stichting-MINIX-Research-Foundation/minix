#!/bin/sh
#
# Copyright (C) 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: tests.sh,v 1.4 2007-06-19 23:47:04 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noauth +noadd +nosea +nostat +noquest +nocomm +nocmd"

status=0

echo "I:checking that master file in the raw format worked"

for server in 1 2
do
	for name in ns mx a aaaa cname dname txt rrsig nsec dnskey ds
	do
		$DIG $DIGOPTS $name.example. $name @10.53.0.$server -p 5300
		echo
	done > dig.out.$server
done

diff dig.out.1 dig.out.2 || status=1

echo "I:exit status: $status"
exit $status
#!/bin/sh
#
# Copyright (C) 2005  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
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

# $Id: tests.sh,v 1.4 2007-06-19 23:47:04 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="+tcp +noauth +noadd +nosea +nostat +noquest +nocomm +nocmd"

status=0

echo "I:checking that master file in the raw format worked"

for server in 1 2
do
	for name in ns mx a aaaa cname dname txt rrsig nsec dnskey ds
	do
		$DIG $DIGOPTS $name.example. $name @10.53.0.$server -p 5300
		echo
	done > dig.out.$server
done

diff dig.out.1 dig.out.2 || status=1

echo "I:exit status: $status"
exit $status
