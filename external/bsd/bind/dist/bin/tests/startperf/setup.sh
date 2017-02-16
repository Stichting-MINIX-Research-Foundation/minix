#!/bin/sh
#
# Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: setup.sh,v 1.4 2011/09/02 21:15:35 each Exp 

usage () {
    echo "Usage: $0 [-s] <number of zones> [<records per zone>]"
    echo "       -s: use the same zone file all zones"
    exit 1
}

if [ "$#" -lt 1 -o "$#" -gt 3 ]; then
    usage
fi

single_file=""
if [ $1 = "-s" ]; then
    single_file=yes
    shift
fi

nzones=$1
shift

nrecords=5
[ "$#" -eq 1 ] && nrecords=$1

. ../system/conf.sh

cat << EOF
options {
        directory "`pwd`";
        listen-on { localhost; };
        listen-on-v6 { localhost; };
	port 5300;
        allow-query { any; };
        allow-transfer { localhost; };
        allow-recursion { none; };
        recursion no;
};

key rndc_key {
        secret "1234abcd8765";
        algorithm hmac-md5;
};

controls {
        inet 127.0.0.1 port 9953 allow { any; } keys { rndc_key; };
};

logging {
        channel basic {
                file "`pwd`/named.log" versions 3 size 100m;
                severity info;
                print-time yes;
                print-severity no;
                print-category no;
        };
        category default {
                basic;
        };
};

EOF

$PERL makenames.pl $nzones | while read zonename; do
    if [ $single_file ]; then
        echo "zone $zonename { type master; file \"smallzone.db\"; };"
    else
        [ -d zones ] || mkdir zones
        $PERL mkzonefile.pl $zonename $nrecords > zones/$zonename.db
        echo "zone $zonename { type master; file \"zones/$zonename.db\"; };"
    fi
done
