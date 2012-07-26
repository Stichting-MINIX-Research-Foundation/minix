#!/bin/sh -e
#
# Copyright (C) 2004, 2006-2011  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2003  Internet Software Consortium.
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

# $Id: sign.sh,v 1.41.40.7 2011-03-21 20:32:15 marka Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=example.
infile=example.db.in
zonefile=example.db

# Have the child generate a zone key and pass it to us.

( cd ../ns3 && sh sign.sh )

for subdomain in secure bogus dynamic keyless nsec3 optout nsec3-unknown \
    optout-unknown multiple rsasha256 rsasha512 kskonly update-nsec3 \
    auto-nsec auto-nsec3 secure.below-cname ttlpatch
do
	cp ../ns3/dsset-$subdomain.example. .
done

keyname1=`$KEYGEN -q -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -g -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null

#
# lower/uppercase the signature bits with the exception of the last characters
# changing the last 4 characters will lead to a bad base64 encoding.
#
$CHECKZONE -D -q -i local $zone $zonefile.signed |
awk '
tolower($1) == "bad-cname.example." && $4 == "RRSIG" && $5 == "CNAME" {
	for (i = 1; i <= NF; i++ ) {
		if (i <= 12) {
			printf("%s ", $i);
			continue;
		}
		prefix = substr($i, 1, length($i) - 4);
		suffix = substr($i, length($i) - 4, 4);
		if (i > 12 && tolower(prefix) != prefix)
			printf("%s%s", tolower(prefix), suffix);
		else if (i > 12 && toupper(prefix) != prefix)
			printf("%s%s", toupper(prefix), suffix);
		else
			printf("%s%s ", prefix, suffix);
	}
	printf("\n");
	next;
}

tolower($1) == "bad-dname.example." && $4 == "RRSIG" && $5 == "DNAME" {
	for (i = 1; i <= NF; i++ ) {
		if (i <= 12) {
			printf("%s ", $i);
			continue;
		}
		prefix = substr($i, 1, length($i) - 4);
		suffix = substr($i, length($i) - 4, 4);
		if (i > 12 && tolower(prefix) != prefix)
			printf("%s%s", tolower(prefix), suffix);
		else if (i > 12 && toupper(prefix) != prefix)
			printf("%s%s", toupper(prefix), suffix);
		else
			printf("%s%s ", prefix, suffix);
	}
	printf("\n");
	next;
}

{ print; }' > $zonefile.signed++ && mv $zonefile.signed++ $zonefile.signed


# Sign the privately secure file

privzone=private.secure.example.
privinfile=private.secure.example.db.in
privzonefile=private.secure.example.db

privkeyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $privzone`

cat $privinfile $privkeyname.key >$privzonefile

$SIGNER -P -g -r $RANDFILE -o $privzone -l dlv $privzonefile > /dev/null

# Sign the DLV secure zone.


dlvzone=dlv.
dlvinfile=dlv.db.in
dlvzonefile=dlv.db

dlvkeyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $dlvzone`

cat $dlvinfile $dlvkeyname.key dlvset-$privzone > $dlvzonefile

$SIGNER -P -g -r $RANDFILE -o $dlvzone $dlvzonefile > /dev/null

# Sign the badparam secure file

zone=badparam.
infile=badparam.db.in
zonefile=badparam.db

keyname1=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone -f KSK $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -3 - -H 1 -g -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null

sed 's/IN NSEC3 1 0 1 /IN NSEC3 1 0 10 /' $zonefile.signed > $zonefile.bad

# Sign the single-nsec3 secure zone with optout

zone=single-nsec3.
infile=single-nsec3.db.in
zonefile=single-nsec3.db

keyname1=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone -f KSK $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -3 - -A -H 1 -g -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null

#
# algroll has just has the old DNSKEY records removed and is waiting
# for them to be flushed from caches.  We still need to generate
# RRSIGs for the old DNSKEY.
#
zone=algroll.
infile=algroll.db.in
zonefile=algroll.db

keyold1=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone -fk $zone`
keyold2=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone`
keynew1=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone -fk $zone`
keynew2=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone`

cat $infile $keynew1.key $keynew2.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone -k $keyold1 -k $keynew1 $zonefile $keyold1 $keyold2 $keynew1 $keynew2 > /dev/null

#
# Make a zone big enough that it takes several seconds to generate a new
# nsec3 chain.
#
zone=nsec3chain-test
zonefile=nsec3chain-test.db
cat > $zonefile << 'EOF'
$TTL 10
@	10	SOA	ns2 hostmaster 0 3600 1200 864000 1200
@	10	NS	ns2
@	10	NS	ns3
ns2	10	A	10.53.0.2
ns3	10	A	10.53.0.3
EOF
awk 'END { for (i = 0; i < 300; i++)
	print "host" i, 10, "NS", "ns.elsewhere"; }' < /dev/null >> $zonefile
key1=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone -fk $zone`
key2=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 1024 -n zone $zone`
cat $key1.key $key2.key >> $zonefile
$SIGNER -P -3 - -A -H 1 -g -r $RANDFILE -o $zone -k $key1 $zonefile $key2 > /dev/null
