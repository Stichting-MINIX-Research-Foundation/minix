#!/bin/sh
#
# Copyright (C) 2004, 2007, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

cp -f ns2/example1.db ns2/example.db
cp -f ns2/named1.conf ns2/named.conf
cp -f ns3/named1.conf ns3/named.conf
rm -f ns2/external/K*
rm -f ns2/external/inline.db.signed
rm -f ns2/external/inline.db.signed.jnl
rm -f ns2/internal/K*
rm -f ns2/internal/inline.db.signed
rm -f ns2/internal/inline.db.signed.jnl

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

#
# We remove k1 and k2 as KEYGEN is deterministic when given the
# same source of "random" data and we want different keys for
# internal and external instances of inline.
#
$KEYGEN -K ns2/internal -r $RANDFILE -3q inline > /dev/null 2>&1
$KEYGEN -K ns2/internal -r $RANDFILE -3qfk inline > /dev/null 2>&1
k1=`$KEYGEN -K ns2/external -r $RANDFILE -3q inline 2> /dev/null`
k2=`$KEYGEN -K ns2/external -r $RANDFILE -3qfk inline 2> /dev/null`
$KEYGEN -K ns2/external -r $RANDFILE -3q inline > /dev/null 2>&1
$KEYGEN -K ns2/external -r $RANDFILE -3qfk inline > /dev/null 2>&1
test -n "$k1" && rm -f ns2/external/$k1.*
test -n "$k2" && rm -f ns2/external/$k2.*
