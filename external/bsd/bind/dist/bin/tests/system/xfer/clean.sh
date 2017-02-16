#!/bin/sh
#
# Copyright (C) 2004, 2007, 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

# Id: clean.sh,v 1.19 2012/02/22 23:47:35 tbox Exp 

#
# Clean up after zone transfer tests.
#

rm -f dig.out.ns1 dig.out.ns2 dig.out.ns3 dig.out.ns4
rm -f dig.out.ns5 dig.out.ns6 dig.out.ns7
rm -f dig.out.soa.ns3
rm -f axfr.out
rm -f ns1/slave.db ns2/slave.db
rm -f ns2/example.db ns2/tsigzone.db ns2/example.db.jnl
rm -f ns3/example.bk ns3/tsigzone.bk ns3/example.bk.jnl
rm -f ns3/master.bk ns3/master.bk.jnl
rm -f ns4/named.conf ns4/nil.db ns4/root.db
rm -f ns6/*.db ns6/*.bk ns6/*.jnl
rm -f ns7/*.db ns7/*.bk ns7/*.jnl

rm -f */named.memstats
rm -f */named.run
rm -f */ans.run
