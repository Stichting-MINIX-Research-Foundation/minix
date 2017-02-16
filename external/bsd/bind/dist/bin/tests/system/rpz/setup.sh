#! /bin/sh
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

set -e

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

QPERF=`$SHELL qperf.sh`

$SHELL clean.sh

# set up test policy zones.
#   bl is the main test zone
#   bl-2 is used to check competing zones.
#   bl-{given,disabled,passthru,no-data,nxdomain,cname,wildcard,garden,
#	    drop,tcp-only} are used to check policy overrides in named.conf.
#   NO-OP is an obsolete synonym for PASSHTRU
for NM in '' -2 -given -disabled -passthru -no-op -nodata -nxdomain -cname -wildcname -garden -drop -tcp-only; do
    sed -e "/SOA/s/blx/bl$NM/g" ns3/base.db >ns3/bl$NM.db
done

# sign the root and a zone in ns2
test -r $RANDFILE || $GENRANDOM 400 $RANDFILE

# $1=directory, $2=domain name, $3=input zone file, $4=output file
signzone () {
    KEYNAME=`$KEYGEN -q -r $RANDFILE -b 512 -K $1 $2`
    cat $1/$3 $1/$KEYNAME.key > $1/tmp
    $SIGNER -Pp -K $1 -o $2 -f $1/$4 $1/tmp >/dev/null
    sed -n -e 's/\(.*\) IN DNSKEY \([0-9]\{1,\} [0-9]\{1,\} [0-9]\{1,\}\) \(.*\)/trusted-keys {"\1" \2 "\3";};/p' $1/$KEYNAME.key >>trusted.conf
    rm dsset-$2 $1/tmp
}
signzone ns2 tld2s. base-tld2s.db tld2s.db


# Performance and a few other checks.
cat <<EOF >ns5/rpz-switch
response-policy {
	zone "bl0"; zone "bl1"; zone "bl2"; zone "bl3"; zone "bl4";
	zone "bl5"; zone "bl6"; zone "bl7"; zone "bl8"; zone "bl9";
	zone "bl10"; zone "bl11"; zone "bl12"; zone "bl13"; zone "bl14";
	zone "bl15"; zone "bl16"; zone "bl17"; zone "bl18"; zone "bl19";
    } recursive-only no
    max-policy-ttl 90
    break-dnssec yes
    qname-wait-recurse no
    ;
EOF

cat <<EOF >ns5/example.db
\$TTL	300
@	SOA	.  hostmaster.ns.example.tld5. ( 1 3600 1200 604800 60 )
	NS	ns
	NS	ns1
ns	A	10.53.0.5
ns1	A	10.53.0.5
EOF

cat <<EOF >ns5/bl.db
\$TTL	300
@		SOA	.  hostmaster.ns.blperf. ( 1 3600 1200 604800 60 )
		NS	ns.tld5.

; for "qname-wait-recurse no" in #35 test1
x.servfail	A	35.35.35.35
; for "recursive-only no" in #8 test5
a3-5.tld2	CNAME	.
; for "break-dnssec" in #9 & #10 test5
a3-5.tld2s	CNAME	.
; for "max-policy-ttl 90" in #17 test5
a3-17.tld2	500 A	17.17.17.17

; dummy NSDNAME policy to trigger lookups
ns1.x.rpz-nsdname	CNAME	.
EOF

if test -n "$QPERF"; then
    # Do not build the full zones if we will not use them.
    $PERL -e 'for ($val = 1; $val <= 65535; ++$val) {
	printf("host-%05d\tA    192.168.%d.%d\n", $val, $val/256, $val%256);
	}' >>ns5/example.db

    echo >>ns5/bl.db
    echo "; rewrite some names" >>ns5/bl.db
    $PERL -e 'for ($val = 2; $val <= 65535; $val += 69) {
	printf("host-%05d.example.tld5\tCNAME\t.\n", $val);
	}' >>ns5/bl.db

    echo >>ns5/bl.db
    echo "; rewrite with some not entirely trivial patricia trees" >>ns5/bl.db
    $PERL -e 'for ($val = 3; $val <= 65535; $val += 69) {
	printf("32.%d.%d.168.192.rpz-ip  \tCNAME\t.\n",
		$val%256, $val/256);
	}' >>ns5/bl.db
fi

# some psuedo-random queryperf requests
$PERL -e 'for ($cnt = $val = 1; $cnt <= 3000; ++$cnt) {
	printf("host-%05d.example.tld5 A\n", $val);
	$val = ($val * 9 + 32771) % 65536;
	}' >ns5/requests

cp ns2/bl.tld2.db.in ns2/bl.tld2.db
cp ns5/empty.db.in ns5/empty.db
cp ns5/empty.db.in ns5/policy2.db
