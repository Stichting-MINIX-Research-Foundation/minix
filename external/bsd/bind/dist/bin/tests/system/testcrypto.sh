#!/bin/sh
#
# Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

SYSTEMTESTTOP=${SYSTEMTESTTOP:=..}
. $SYSTEMTESTTOP/conf.sh

test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

prog=$0

args="-r $RANDFILE"
alg="-a RSAMD5 -b 512"
quiet=0

msg1="cryptography"
msg2="--with-openssl, or --enable-native-pkcs11 --with-pkcs11"
while test "$#" -gt 0; do
        case $1 in
        -q)
                args="$args -q"
                quiet=1
                ;;
        rsa|RSA)
                alg=""
                msg1="RSA cryptography"
                ;;
        gost|GOST)
                alg="-a eccgost"
                msg1="GOST cryptography"
                msg2="--with-gost"
                ;;
        ecdsa|ECDSA)
                alg="-a ecdsap256sha256"
                msg1="ECDSA cryptography"
                msg2="--with-ecdsa"
                ;;
        *)
                echo "${prog}: unknown argument"
                exit 1
                ;;
        esac
        shift
done


if $KEYGEN $args $alg foo > /dev/null 2>&1
then
    rm -f Kfoo*
else
    if test $quiet -eq 0; then
        echo "I:This test requires support for $msg1" >&2
        echo "I:configure with $msg2" >&2
    fi
    exit 255
fi
