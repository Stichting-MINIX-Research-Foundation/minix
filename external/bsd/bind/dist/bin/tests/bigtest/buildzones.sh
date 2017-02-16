#!/bin/bash
#
# Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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
. ../conf.sh

addr=127.127.0.0
ttl=300
named=${NAMED}
keygen=${KEYGEN}
dsfromkey=${DSFROMKEY}

nextaddr() {
	OLDIF="$IFS"
	IFS="${IFS}."
	set $1
	IFS="$OLDIFS"
	_a=$1 _b=$2 _c=$3 _d=$4
	_d=$(($_d + 1))
	case $_d in
	256) _c=$(($_c + 1)); _d=0;;
	esac
	case $_c in
	256) _b=$(($_b + 1)); _c=0;;
	esac
	echo $_a.$_b.$_c.$_d
}

parent() {
	OLDIF="$IFS"
	IFS="${IFS}."
	set $1
	IFS="$OLDIFS"
	shift
	while [ $# -ne 0 ]
	do
		printf %s ${1}
		shift
		printf %s ${1:+.}
		
	done
}

blackhole() {
	echo 'options {'
	echo '	port 5300;'
	echo "	listen-on { $1; };"
	echo "	query-source $1;"
	echo "	notify-source $1;"
	echo "	transfer-source $1;"
	echo '	key-directory "keys";'
	echo "	recursion ${2:-no};"
	echo '	pid-file "pids/'"${addr}"'.pid";'
	echo '	blackhole { 127.127.0.0; };'
	echo '};'
}

refuse() {
	echo 'options {'
	echo '	port 5300;'
	echo "	listen-on { $1; };"
	echo "	query-source $1;"
	echo "	notify-source $1;"
	echo "	transfer-source $1;"
	echo '	key-directory "keys";'
	echo "	recursion ${2:-no};"
	echo '	pid-file "pids/'"${addr}"'.pid";'
	echo '	allow-query { !127.127.0.0; any; };'
	echo '};'
}

options() {
	echo 'options {'
	echo '	port 5300;'
	echo "	listen-on { $1; };"
	echo "	query-source $1;"
	echo "	notify-source $1;"
	echo "	transfer-source $1;"
	echo '	key-directory "keys";'
	echo "	recursion ${2:-no};"
	echo '	pid-file "pids/'"${addr}"'.pid";'
	echo '};'
}

controls() {
       echo 'include "rndc.key";'
       echo "controls { inet $addr port 9953 allow { any; } keys { "rndc-key"; }; };"
}

delay() {
	_s=$1
	OLDIF="$IFS"
	IFS="${IFS}/"
	set ${2:-.}
	IFS="$OLDIFS"

	case $1 in
	.) _d=;;
	*) _d=$1;;
	esac
	case $_s in
	1) echo -T delay=${_d:-100};;
	2) echo -T delay=${2:-50};;
	3) echo -T delay=${3:-150};;
	4) echo -T delay=${4:-250};;
	5) echo -T delay=${5:-125};;
	6) echo -T delay=${6:-25};;
	7) echo -T delay=${7:-75};;
	8) echo -T delay=${8:-125};;
	9) echo -T delay=${9:-10};;
	10) echo -T delay=${10:-40};;
	11) echo -T delay=${11:-80};;
	12) echo -T delay=${12:-90};;
	*) echo -T delay=50;;
	esac
}

trusted-keys () {
	awk '$3 == "DNSKEY" {
	        b = ""; for (i=7; i <= NF; i++) { b = b $i; };
		print "trusted-keys { \""$1"\"",$4,$5,$6,"\""b"\"; };" };'
}

signed-zone () {
	echo "zone "'"'"${1:-.}"'"'" {"
	echo "	type master;"
	echo "	file "'"'"master/${2}.db"'"'";"
	echo "	auto-dnssec maintain;"
	echo "	allow-update { any; };"
	echo "};"
}

unsigned-zone () {
	echo "zone "'"'"${1:-.}"'"'" {"
	echo "	type master;"
	echo "	file "'"'"master/${2}.db"'"'";"
	echo "};"
}

slave-zone () {
	echo "zone "'"'"${zone:-.}"'"'" {"
	echo "	type slave;"
	echo "	masters { ${master}; };"
	echo "};"
}

rm -rf servers master keys setup teardown run 
mkdir -p servers
mkdir -p master
mkdir -p keys

echo "ifconfig lo0 $addr netmask 0xffffffff alias" >> setup
echo "ifconfig lo0 $addr -alias" >> teardown
controls $addr > named.conf
options $addr yes >> named.conf
echo 'zone "." { type hint; file "master/hint.db"; };' >> named.conf

while read zone servers nsfmt signed delay blackhole refuse flags
do
	i=1
	case "${zone}" in
	.) file=root zone=;;
	*) file="$zone";;
	esac
	if [ "${zone}" != "" ] ; then
		p=$(parent $zone)
		case "${p}" in
		"") p=root;;
		esac
	else
		p=hint
	fi
	#echo "zone='${zone}' parent='${p}'"
	addr=$(nextaddr $addr)
	ns=$(printf "$nsfmt" ${i} "${zone}")
	d=$(delay $i ${delay:-.})

	echo "${zone}. ${ttl} soa ${ns}. hostmaster.${zone}${zone:+.} 1 3600 1200 604800 1200" >> master/${file}.db
	echo "${zone}. ${ttl} ns ${ns}." >> master/${file}.db
	echo "${ns}. ${ttl} a ${addr}" >> master/${file}.db
	echo "${zone}. ${ttl} ns ${ns}." >> master/${p}.db
	echo "${ns}. ${ttl} a ${addr}" >> master/${p}.db
	if [ $signed = "S" ]; then
		kskkey=`${keygen} -K keys -f KSK ${zone:-.}`
		zskkey=`${keygen} -K keys ${zone:-.}`
		if [ "${zone}" != "" ] ; then
			${dsfromkey} -T ${ttl} keys/${kskkey}.key >> master/${p}.db
		else
			trusted-keys <  keys/${kskkey}.key >> named.conf
		fi
	fi
	echo "ifconfig lo0 $addr netmask 0xffffffff alias" >> setup
	echo "ifconfig lo0 $addr -alias" >> teardown
	echo "${named} -D bigtest -c servers/${addr}.conf $d $flags" >> run
	options ${addr} > servers/${addr}.conf
	case ${signed} in
	S) signed-zone ${zone:-.} ${file} >> servers/${addr}.conf;;
	P) unsigned-zone ${zone:-.} ${file} >> servers/${addr}.conf;;
	*) echo ${signed}; exit 1;;
	esac

	# slave servers
	while [ $i -lt $servers ]
	do
		master=$addr
		i=$(($i + 1))
		ns=$(printf "$nsfmt" ${i} "${zone}")
		d=$(delay $i ${delay:-.})
		addr=$(nextaddr $addr)
		echo "${zone}. ${ttl} ns ${ns}." >> master/${file}.db
		echo "${ns}. ${ttl} a ${addr}" >> master/${file}.db
		echo "${zone}. ${ttl} ns ${ns}." >> master/${p}.db
		echo "${ns}. ${ttl} a ${addr}" >> master/${p}.db
		echo "ifconfig lo0 $addr netmask 0xffffffff alias" >> setup
		echo "ifconfig lo0 $addr -alias" >> teardown
		echo "${named} -D bigtest -c servers/${addr}.conf $d $flags" >> run
		if [ $i = ${refuse:-.} ]
		then
			refuse $addr > servers/${addr}.conf
		elif [ $i = ${blackhole:-.} ]
		then
			blackhole $addr > servers/${addr}.conf
		else
			options $addr > servers/${addr}.conf
		fi
		slave-zone ${zone:-.} ${master} >> servers/${addr}.conf
	done
	if [ "${zone}" != "" ] ; then
		echo "www.${zone}. ${ttl} a 127.0.0.1" >> master/${file}.db
		echo "www.${zone}. ${ttl} aaaa ::1" >> master/${file}.db
		echo "${zone}. ${ttl} mx 10 mail.${zone}." >> master/${file}.db
		echo "mail.${zone}. ${ttl} a 127.0.0.1" >> master/${file}.db
		echo "mail.${zone}. ${ttl} aaaa ::1" >> master/${file}.db
		echo "*.big.${zone}. ${ttl} txt (" >> master/${file}.db
		i=0
		while [ $i -lt 150 ]
		do
			echo "1234567890" >> master/${file}.db
			i=$(($i + 1))
		done
		echo ")" >> master/${file}.db
		echo "*.medium.${zone}. ${ttl} txt (" >> master/${file}.db
		i=0
		while [ $i -lt 120 ]
		do
			echo "1234567890" >> master/${file}.db
			i=$(($i + 1))
		done
		echo ")" >> master/${file}.db
		echo "*.medium.${zone}. ${ttl} txt (" >> master/${file}.db
		i=0
		while [ $i -lt 120 ]
		do
			echo "1234567890" >> master/${file}.db
			i=$(($i + 1))
		done
		echo ")" >> master/${file}.db
	fi
done
