#!/bin/sh -e
#
# Copyright (C) 2009, 2011  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: setup.sh,v 1.3.250.2 2011-03-21 23:46:58 tbox Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh
. ./clean.sh

RANDFILE=./random.data

pzone=parent.nil
czone=child.parent.nil

../../../tools/genrandom 400 $RANDFILE

echo "I:generating keys"

# active zsk
zsk=`$KEYGEN -q -r $RANDFILE $czone`
echo $zsk > zsk.key

# not yet published or active
pending=`$KEYGEN -q -r $RANDFILE -P none -A none $czone`
echo $pending > pending.key

# published but not active
standby=`$KEYGEN -q -r $RANDFILE -A none $czone`
echo $standby > standby.key

# inactive
inact=`$KEYGEN -q -r $RANDFILE -P now-24h -A now-24h -I now $czone`
echo $inact > inact.key

# active ksk
ksk=`$KEYGEN -q -r $RANDFILE -fk $czone`
echo $ksk > ksk.key

# published but not YET active; will be active in 15 seconds
rolling=`$KEYGEN -q -r $RANDFILE -fk $czone`
$SETTIME -A now+15s $rolling > /dev/null
echo $rolling > rolling.key

# revoked
revoke1=`$KEYGEN -q -r $RANDFILE -fk $czone`
echo $revoke1 > prerev.key
revoke2=`$REVOKE $revoke1`
echo $revoke2 | sed -e 's#\./##' -e "s/\.key.*$//" > postrev.key

pzsk=`$KEYGEN -q -r $RANDFILE $pzone`
echo $pzsk > parent.zsk.key

pksk=`$KEYGEN -q -r $RANDFILE -fk $pzone`
echo $pksk > parent.ksk.key

oldstyle=`$KEYGEN -Cq -r $RANDFILE $pzone`
echo $oldstyle > oldstyle.key

