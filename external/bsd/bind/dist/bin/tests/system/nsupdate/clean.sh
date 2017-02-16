#!/bin/sh
#
# Copyright (C) 2004, 2007, 2009-2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

#
# Clean up after zone transfer tests.
#

rm -f ns1/*.jnl ns2/*.jnl
rm -f ns1/example.db ns1/unixtime.db ns1/update.db ns1/other.db ns1/keytests.db
rm -f ns1/md5.key ns1/sha1.key ns1/sha224.key ns1/sha256.key ns1/sha384.key
rm -f ns1/sha512.key ns1/ddns.key
rm -f nsupdate.out
rm -f ns2/example.bk
rm -f ns2/update.bk ns2/update.alt.bk
rm -f */named.memstats
rm -f nsupdate.out
rm -f ns3/example.db.jnl ns3/example.db
rm -f ns3/nsec3param.test.db.signed.jnl ns3/nsec3param.test.db ns3/nsec3param.test.db.signed ns3/dsset-nsec3param.test.
rm -f ns3/dnskey.test.db.signed.jnl ns3/dnskey.test.db ns3/dnskey.test.db.signed ns3/dsset-dnskey.test.
rm -f ns3/K*
rm -f dig.out.*
rm -f jp.out.ns3.*
rm -f Kxxx.*
rm -f typelist.out.*
