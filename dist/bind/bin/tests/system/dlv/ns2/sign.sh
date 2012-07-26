#!/bin/sh
#
# Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: sign.sh,v 1.2.2.3 2011-05-26 23:47:05 tbox Exp $

(cd ../ns3 && sh -e ./sign.sh || exit 1)

echo "I:dlv/ns2/sign.sh"

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=druz.
infile=druz.db.in
zonefile=druz.db
outfile=druz.pre
dlvzone=utld.

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null` 
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone 2> /dev/null`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -r $RANDFILE -l $dlvzone -g -o $zone -f $outfile $zonefile > /dev/null 2> signer.err || cat signer.err

$CHECKZONE -q -D -i none druz druz.pre |
sed '/IN DNSKEY/s/\([a-z0-9A-Z/]\{10\}\)[a-z0-9A-Z/]\{16\}/\1XXXXXXXXXXXXXXXX/'> druz.signed

echo "I: signed $zone"
