#!/bin/sh
#
# Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: clean.sh,v 1.7.16.3 2011-07-08 01:45:58 each Exp $

rm -f */K* */dsset-* */*.signed */trusted.conf */tmp* */*.jnl */*.bk
rm -f active.key inact.key del.key unpub.key standby.key rev.key
rm -f nopriv.key vanishing.key del1.key del2.key
rm -f delayksk.key delayzsk.key missingzsk.key inactivezsk.key
rm -f nsupdate.out
rm -f */core
rm -f */example.bk
rm -f */named.memstats
rm -f dig.out.*
rm -f random.data
rm -f ns1/root.db
rm -f ns2/example.db
rm -f ns2/private.secure.example.db ns2/bar.db
rm -f ns3/nsec.example.db
rm -f ns3/nsec3.example.db
rm -f ns3/nsec3.nsec3.example.db
rm -f ns3/nsec3.optout.example.db
rm -f ns3/nsec3-to-nsec.example.db
rm -f ns3/oldsigs.example.db
rm -f ns3/optout.example.db
rm -f ns3/optout.nsec3.example.db
rm -f ns3/optout.optout.example.db
rm -f ns3/rsasha256.example.db ns3/rsasha512.example.db
rm -f ns3/secure.example.db
rm -f ns3/secure.nsec3.example.db
rm -f ns3/secure.optout.example.db
rm -f ns3/secure-to-insecure.example.db
rm -f ns3/nozsk.example.db ns3/inaczsk.example.db
rm -f ns3/prepub.example.db
rm -f ns3/prepub.example.db.in
rm -f ns3/secure-to-insecure2.example.db
