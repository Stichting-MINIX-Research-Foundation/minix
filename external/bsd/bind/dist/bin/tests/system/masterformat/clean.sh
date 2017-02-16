#!/bin/sh
#
# Copyright (C) 2005, 2007, 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

rm -f named-compilezone
rm -f ns1/example.db.raw*
rm -f ns1/example.db.compat
rm -f ns1/example.db.serial.raw
rm -f ns1/large.db ns1/large.db.raw
rm -f ns1/example.db.map ns1/signed.db.map
rm -f ns1/session.key
rm -f dig.out.*
rm -f dig.out
rm -f */named.memstats
rm -f ns2/example.db
rm -f ns2/transfer.db.*
rm -f ns2/formerly-text.db
rm -f ns2/db-*
rm -f ns2/large.bk
rm -f ns3/example.db.map ns3/dynamic.db.map
rm -f baseline.txt text.1 text.2 raw.1 raw.2 map.1 map.2 map.5 text.5 badmap
rm -f ns1/Ksigned.* ns1/dsset-signed. ns1/signed.db.signed
rm -f rndc.out
