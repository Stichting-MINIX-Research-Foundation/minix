#!/bin/sh
#
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

$SHELL ../genzone.sh 2 >ns2/nil.db
$SHELL ../genzone.sh 2 >ns2/other.db
$SHELL ../genzone.sh 2 >ns2/static.db

cat ns4/named.conf.in > ns4/named.conf

make_key () {
    $RNDCCONFGEN -r $RANDFILE -k key$1 -A $2 -s 10.53.0.4 -p 995${1} \
            > ns4/key${1}.conf
    egrep -v '(^# Start|^# End|^# Use|^[^#])' ns4/key$1.conf | cut -c3- | \
            sed 's/allow { 10.53.0.4/allow { any/' >> ns4/named.conf
}

make_key 1 hmac-md5
make_key 2 hmac-sha1
make_key 3 hmac-sha224
make_key 4 hmac-sha256
make_key 5 hmac-sha384
make_key 6 hmac-sha512
