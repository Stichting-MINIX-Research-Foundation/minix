#!/bin/sh
# Copyright (C) 2010, 2012  Internet Systems Consortium, Inc. ("ISC")
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
# Generate a DNS RR from an x.509 certificate
# Currently only supports TLSA, but can be extended to support
# other DANE types such as SMIMEA in the future.
#
# Requires: openssl

USAGE="$BASENAME [options] <filename>
Options:
        -f <input format>: PEM | DLR
        -n <name>: record name (default: _443._tcp)
        -o <origin>: zone origin (default: none; name will be relative)
        -m <matching type>: NONE (0) | SHA256 (1) | SHA512 (2)
        -r <RR type>: TLSA
        -s <selector>: FULL (0) | PK (1)
        -t <ttl>: TTL of the TLSA record (default: none)
        -u <certificate usage>: CA (0) | SERVICE (1) | TA (2) | DOMAIN (3)"

NM="_443._tcp"
CU=2
SELECTOR=0
MTYPE=1
IN=
FORM=PEM
TTL=
RRTYPE=TLSA
BASENAME=`basename $0`;

while getopts "xn:o:u:s:t:m:i:f:r:" c; do
    case $c in
	x) set -x; DEBUG=-x;;
	m) MTYPE="$OPTARG";;
        n) NM="$OPTARG";;
        o) ORIGIN="$OPTARG";;
        r) RRTYPE="$OPTARG";;
	s) SELECTOR="$OPTARG";;
        t) TTL="$OPTARG";;
	u) CU="$OPTARG";;
	*) echo "$USAGE" 1>&2; exit 1;;
    esac
done
shift `expr $OPTIND - 1 || true`

if test "$#" -eq 1; then
    IN=$1
else
    echo "$USAGE" 1>&2; exit 1
fi

ORIGIN=`echo $ORIGIN | sed 's/\([^.]$\)/\1./'`
if [ -n "$ORIGIN" ]; then
    NM=`echo $NM | sed 's/\.$//'`
    NM="$NM.$ORIGIN"
fi

case "$CU" in
    [Cc][Aa]) CU=0;;
    [Ss][Ee][Rr][Vv]*) CU=1;;
    [Tt][Aa]) CU=2;;
    [Dd][Oo][Mm]*) CU=3;;
    [0123]) ;;
    *) echo "bad certificate usage -u \"$CU\"" 1>&2; exit 1;;
esac

case "$SELECTOR" in
    [Ff][Uu][Ll][Ll]) SELECTOR=0;;
    [Pp][Kk]) SELECTOR=1;;
    [01]) ;;
    *) echo "bad selector -s \"$SELECTOR\"" 1>&2; exit 1;;
esac

case "$MTYPE" in
    0|[Nn][Oo][Nn][Ee]) HASH='od -A n -v -t xC';;
    1|[Ss][Hh][Aa]256) HASH='openssl dgst -sha256';;
    2|[Ss][Hh][Aa]512) HASH='openssl dgst -sha512';;
    *) echo "bad matching type -m \"$MTYPE\"" 1>&2; exit 1;;
esac

case "$FORM" in
    [Pp][Ee][Mm]) FORM=PEM;;
    [Dd][Ll][Rr]) FORM=DLR;;
    *) echo "bad input file format -f \"$FORM\"" 1>&2; exit 1
esac

case "$RRTYPE" in
    [Tt][Ll][Ss][Aa]) RRTYPE=TLSA;;
    *) echo "invalid RR type" 1>&2; exit 1
esac

if test -z "$IN" -o ! -s "$IN"; then
    echo "bad input file -i \"$IN\"" 1>&2; exit 1
fi

echo "; $BASENAME -o$NM -u$CU -s$SELECTOR -m$MTYPE -f$FORM $IN"

(if test "$SELECTOR" = 0; then
    openssl x509 -in "$IN" -inform "$FORM" -outform DER
else
    openssl x509 -in "$IN" -inform "$FORM" -noout -pubkey		\
	| sed -e '/PUBLIC KEY/d'					\
	| openssl base64 -d 
fi)									\
    | $HASH								\
    | awk '
	# format Association Data as in Appendix C of the DANE RFC
	BEGIN {
		print "'"$NM\t\t$TTL\tIN TLSA\t$CU $SELECTOR $MTYPE"' ("; 
		leader = "\t\t\t\t\t"; 
	}
	/.+/ {
	    gsub(/ +/, "", $0);
	    buf = buf $0;
	    while (length(buf) >= 36) {
		print leader substr(buf, 1, 36);
		buf = substr(buf, 37);
	    }
	}
	END {
            if (length(buf) > 34)
                print leader buf "\n" leader ")";
            else
                print leader buf " )";
        }'
