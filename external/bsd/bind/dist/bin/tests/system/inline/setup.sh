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

$SHELL clean.sh

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

cp ns1/root.db.in ns1/root.db
rm -f ns1/root.db.signed

touch ns2/trusted.conf
cp ns2/bits.db.in ns2/bits.db
cp ns2/bits.db.in ns2/retransfer.db
cp ns2/bits.db.in ns2/retransfer3.db
rm -f ns2/bits.db.jnl

cp ns3/master.db.in ns3/master.db
cp ns3/master.db.in ns3/dynamic.db
cp ns3/master.db.in ns3/updated.db
cp ns3/master.db.in ns3/expired.db
cp ns3/master.db.in ns3/nsec3.db
cp ns3/master.db.in ns3/externalkey.db

touch ns4/trusted.conf
cp ns4/noixfr.db.in ns4/noixfr.db
rm -f ns4/noixfr.db.jnl

cp ns5/named.conf.pre ns5/named.conf

(cd ns3; $SHELL -e sign.sh)
(cd ns1; $SHELL -e sign.sh)
