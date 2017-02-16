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

# Clean up after rpz tests.

rm -f proto.* dsset-* trusted.conf dig.out* nsupdate.tmp ns*/*tmp
rm -f ns*/*.key ns*/*.private ns2/tld2s.db ns2/bl.tld2.db
rm -f ns3/bl*.db ns*/*switch ns*/empty.db ns*/empty.db.jnl
rm -f ns5/requests ns5/example.db ns5/bl.db ns5/*.perf
rm -f */named.memstats */named.run */named.stats */session.key
rm -f */*.jnl */*.core */*.pid
rm -f */policy2.db
