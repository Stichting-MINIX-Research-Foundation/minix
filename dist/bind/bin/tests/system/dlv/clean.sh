#!/bin/sh
#
# Copyright (C) 2004, 2007, 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: clean.sh,v 1.7.120.2 2011-05-26 23:47:05 tbox Exp $

rm -f random.data
rm -f ns*/named.run
rm -f ns1/K*
rm -f ns1/dsset-*
rm -f ns1/*.signed
rm -f ns1/signer.err
rm -f ns1/root.db
rm -f ns2/K*
rm -f ns2/dlvset-*
rm -f ns2/dsset-*
rm -f ns2/*.signed
rm -f ns2/*.pre
rm -f ns2/signer.err
rm -f ns2/druz.db
rm -f ns3/K*
rm -f ns3/*.db
rm -f ns3/*.signed
rm -f ns3/dlvset-*
rm -f ns3/dsset-*
rm -f ns3/keyset-*
rm -f ns1/trusted.conf ns5/trusted.conf
rm -f ns3/trusted-dlv.conf ns5/trusted-dlv.conf
rm -f ns3/signer.err
rm -f ns6/K*
rm -f ns6/*.db
rm -f ns6/*.signed
rm -f ns6/dsset-*
rm -f ns6/signer.err
rm -f */named.memstats
rm -f dig.out.ns*.test*
