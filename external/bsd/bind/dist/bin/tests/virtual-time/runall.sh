#!/bin/sh
#
# Copyright (C) 2010, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: runall.sh,v 1.2 2010/06/17 05:38:05 marka Exp 

#
# Run all the virtual time tests.
#

SYSTEMTESTTOP=.
. $SYSTEMTESTTOP/conf.sh

$PERL testsock.pl || {
	echo "I:Network interface aliases not set up.  Skipping tests." >&2;
	echo "R:UNTESTED" >&2;
	echo "E:virtual-time:`date`" >&2;
	exit 0;
}

status=0

for d in $SUBDIRS
do
	sh run.sh $d || status=1
done

exit $status
