#!/bin/sh
#
# Copyright (C) 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

$SHELL clean.sh

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

$SHELL ../genzone.sh 1 > ns1/master.db
$SHELL ../genzone.sh 1 > ns1/duplicate.db
cd ns1
touch master.db.signed
echo '$INCLUDE "master.db.signed"' >> master.db
$KEYGEN -r $RANDFILE -3q master.example > /dev/null 2>&1
$KEYGEN -r $RANDFILE -3qfk master.example > /dev/null 2>&1
$SIGNER -SD -o master.example master.db > /dev/null 2>&1
echo '$INCLUDE "soa.db"' > reload.db
echo '@ 0 NS .' >> reload.db
echo '@ 0 SOA . . 1 0 0 0 0' > soa.db
