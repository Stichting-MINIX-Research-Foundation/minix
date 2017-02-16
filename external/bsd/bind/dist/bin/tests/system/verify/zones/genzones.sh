# Copyright (C) 2012-2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

dumpit () {
	echo "D:${debug}: dumping ${1}"
	cat "${1}" | sed 's/^/D:/'
}
setup () {
	echo "I:setting up $2 zone: $1"
	debug="$1"
	zone="$1"
	file="$1.$2"
	n=`expr ${n:-0} + 1`
}

# A unsigned zone should fail validation.
setup unsigned bad
cp unsigned.db unsigned.bad

# A set of nsec zones.
setup zsk-only.nsec good
$KEYGEN -r $RANDFILE ${zone}> kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -SP -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk-only.nsec good
$KEYGEN -r $RANDFILE -fK ${zone} > kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -SPz -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk+zsk.nsec good
$KEYGEN -r $RANDFILE ${zone} > kg1.out$n 2>&1 || dumpit kg1.out$n
$KEYGEN -r $RANDFILE -fK ${zone} > kg2.out$n 2>&1 || dumpit kg2.out$n
$SIGNER -SPx -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

# A set of nsec3 zones.
setup zsk-only.nsec3 good
$KEYGEN -3 -r $RANDFILE ${zone}> kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -3 - -SP -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk-only.nsec3 good
$KEYGEN -3 -r $RANDFILE -fK ${zone} > kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -3 - -SPz -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk+zsk.nsec3 good
$KEYGEN -3 -r $RANDFILE ${zone} > kg1.out$n 2>&1 || dumpit kg1.out$n
$KEYGEN -3 -r $RANDFILE -fK ${zone} > kg2.out$n 2>&1 || dumpit kg2.out$n
$SIGNER -3 - -SPx -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk+zsk.outout good
$KEYGEN -3 -r $RANDFILE ${zone} > kg1.out$n 2>&1 || dumpit kg1.out$n
$KEYGEN -3 -r $RANDFILE -fK ${zone} > kg2.out$n 2>&1 || dumpit kg2.out$n
$SIGNER -3 - -A -SPx -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

# A set of zones with only DNSKEY records.
setup zsk-only.dnskeyonly bad
key1=`$KEYGEN -r $RANDFILE ${zone} 2>kg.out` || dumpit kg.out$n
cat unsigned.db $key1.key > ${file}

setup ksk-only.dnskeyonly bad
key1=`$KEYGEN -r $RANDFILE -fK ${zone} 2>kg.out` || dumpit kg.out$n
cat unsigned.db $key1.key > ${file}

setup ksk+zsk.dnskeyonly bad
key1=`$KEYGEN -r $RANDFILE ${zone} 2>kg.out` || dumpit kg.out$n
key2=`$KEYGEN -r $RANDFILE -fK ${zone} 2>kg.out` || dumpit kg.out$n
cat unsigned.db $key1.key $key2.key > ${file}

# A set of zones with expired records
s="-s -2678400"
setup zsk-only.nsec.expired bad
$KEYGEN -r $RANDFILE ${zone}> kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -SP ${s} -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk-only.nsec.expired bad
$KEYGEN -r $RANDFILE -fK ${zone} > kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -SPz ${s} -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk+zsk.nsec.expired bad
$KEYGEN -r $RANDFILE ${zone} > kg1.out$n 2>&1 || dumpit kg1.out$n
$KEYGEN -r $RANDFILE -fK ${zone} > kg2.out$n 2>&1 || dumpit kg2.out$n
$SIGNER -SP ${s} -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup zsk-only.nsec3.expired bad
$KEYGEN -3 -r $RANDFILE ${zone}> kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -3 - ${s} -SP -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk-only.nsec3.expired bad
$KEYGEN -3 -r $RANDFILE -fK ${zone} > kg.out$n 2>&1 || dumpit kg.out$n
$SIGNER -3 - ${s} -SPz -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

setup ksk+zsk.nsec3.expired bad
$KEYGEN -3 -r $RANDFILE ${zone} > kg1.out$n 2>&1 || dumpit kg1.out$n
$KEYGEN -3 -r $RANDFILE -fK ${zone} > kg2.out$n 2>&1 || dumpit kg2.out$n
$SIGNER -3 - ${s} -SPx -o ${zone} -f ${file} unsigned.db > s.out$n 2>&1 || dumpit s.out$n

# ksk expired
setup ksk+zsk.nsec.ksk-expired bad
zsk=`$KEYGEN -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -Px -o ${zone} -f ${file} ${file} $zsk > s.out$n 2>&1 || dumpit s.out$n
$SIGNER ${s} -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
now=`date -u +%Y%m%d%H%M%S`
exp=`awk '$4 == "RRSIG" && $5 == "DNSKEY" { print $9;}' ${file}`
[ "${exp:-40001231246060}" -lt ${now:-0} ] || dumpit $file

setup ksk+zsk.nsec3.ksk-expired bad
zsk=`$KEYGEN -3 -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -3 -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -3 - -Px -o ${zone} -f ${file} ${file} $zsk > s.out$n 2>&1 || dumpit s.out$n
$SIGNER -3 - ${s} -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
now=`date -u +%Y%m%d%H%M%S`
exp=`awk '$4 == "RRSIG" && $5 == "DNSKEY" { print $9;}' ${file}`
[ "${exp:-40001231246060}" -lt ${now:-0} ] || dumpit $file

# broken nsec chain
setup ksk+zsk.nsec.broken-chain bad
zsk=`$KEYGEN -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
awk '$4 == "NSEC" { $5 = "'$zone'."; print } { print }' ${file} > ${file}.tmp
$SIGNER -Px -Z nonsecify -o ${zone} -f ${file} ${file}.tmp $zsk > s.out$n 2>&1 || dumpit s.out$n

# bad nsec bitmap
setup ksk+zsk.nsec.bad-bitmap bad
zsk=`$KEYGEN -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
awk '$4 == "NSEC" && /SOA/ { $6=""; print } { print }' ${file} > ${file}.tmp
$SIGNER -Px -Z nonsecify -o ${zone} -f ${file} ${file}.tmp $zsk > s.out$n 2>&1 || dumpit s.out$n

# extra NSEC record out side of zone
setup ksk+zsk.nsec.out-of-zone-nsec bad
zsk=`$KEYGEN -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
echo "out-of-zone. 3600 IN NSEC ${zone}. A" >> ${file}
$SIGNER -Px -Z nonsecify -O full -o ${zone} -f ${file} ${file} $zsk > s.out$n 2>&1 || dumpit s.out$n

# extra NSEC record below bottom of one
setup ksk+zsk.nsec.below-bottom-of-zone-nsec bad
zsk=`$KEYGEN -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
echo "ns.sub.${zone}. 3600 IN NSEC ${zone}. A AAAA" >> ${file}
$SIGNER -Px -Z nonsecify -O full -o ${zone} -f ${file}.tmp ${file} $zsk > s.out$n 2>&1 || dumpit s.out$n
# dnssec-signzone signs any node with a NSEC record.
awk '$1 ~ /^ns.sub/ && $4 == "RRSIG" && $5 != "NSEC" { next; } { print; }' ${file}.tmp > ${file}

# missing NSEC3 record at empty node
# extract the hash fields from the empty node's NSEC 3 record then fix up
# the NSEC3 chain to remove it
setup ksk+zsk.nsec3.missing-empty bad
zsk=`$KEYGEN -3 -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -3 -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -3 - -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
a=`awk '$4 == "NSEC3" && NF == 9 { split($1, a, "."); print a[1]; }' ${file}`
b=`awk '$4 == "NSEC3" && NF == 9 { print $9; }' ${file}`
awk '
$4 == "NSEC3" && $9 == "'$a'" { $9 = "'$b'"; print; next; }
$4 == "NSEC3" && NF == 9 { next; }
{ print; }' ${file} > ${file}.tmp
$SIGNER -3 - -Px -Z nonsecify -O full -o ${zone} -f ${file} ${file}.tmp $zsk > s.out$n 2>&1 || dumpit s.out$n

# extra NSEC3 record
setup ksk+zsk.nsec3.extra-nsec3 bad
zsk=`$KEYGEN -3 -r $RANDFILE ${zone} 2> kg1.out$n` || dumpit kg1.out$n
ksk=`$KEYGEN -3 -r $RANDFILE -fK ${zone} 2> kg2.out$n` || dumpit kg2.out$n
cat unsigned.db $ksk.key $zsk.key > $file
$SIGNER -3 - -P -O full -o ${zone} -f ${file} ${file} $ksk > s.out$n 2>&1 || dumpit s.out$n
awk '
BEGIN {
	ZONE="'${zone}'.";
}
$4 == "NSEC3" && NF == 9 {
	$1 = "H9P7U7TR2U91D0V0LJS9L1GIDNP90U3H." ZONE;
	$9 = "H9P7U7TR2U91D0V0LJS9L1GIDNP90U3I";
	print;
}' ${file} >> ${file}
$SIGNER -3 - -Px -Z nonsecify -O full -o ${zone} -f ${file} ${file} $zsk > s.out$n 2>&1 || dumpit s.out$n
