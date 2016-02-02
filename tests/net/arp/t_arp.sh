#	$NetBSD: t_arp.sh,v 1.9 2015/08/31 08:08:20 ozaki-r Exp $
#
# Copyright (c) 2015 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

inetserver="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpnet_shmif"
HIJACKING="env LD_PRELOAD=/usr/lib/librumphijack.so RUMPHIJACK=sysctl=yes"

SOCKSRC=unix://commsock1
SOCKDST=unix://commsock2
IP4SRC=10.0.1.1
IP4DST=10.0.1.2

DEBUG=false
TIMEOUT=1

atf_test_case cache_expiration_5s cleanup
atf_test_case cache_expiration_10s cleanup
atf_test_case command cleanup
atf_test_case garp cleanup
atf_test_case cache_overwriting cleanup

cache_expiration_5s_head()
{
	atf_set "descr" "Tests for ARP cache expiration (5s)"
	atf_set "require.progs" "rump_server"
}

cache_expiration_10s_head()
{
	atf_set "descr" "Tests for ARP cache expiration (10s)"
	atf_set "require.progs" "rump_server"
}

command_head()
{
	atf_set "descr" "Tests for commands of arp(8)"
	atf_set "require.progs" "rump_server"
}

garp_head()
{
	atf_set "descr" "Tests for GARP"
	atf_set "require.progs" "rump_server"
}

cache_overwriting_head()
{
	atf_set "descr" "Tests for behavior of overwriting ARP caches"
	atf_set "require.progs" "rump_server"
}

setup_dst_server()
{
	export RUMP_SERVER=$SOCKDST
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet $IP4DST/24
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 rump.ifconfig -w 10

	$DEBUG && rump.ifconfig shmif0
	$DEBUG && rump.arp -n -a
}

setup_src_server()
{
	local keep=$1

	export RUMP_SERVER=$SOCKSRC

	# Adjust ARP parameters
	atf_check -s exit:0 -o ignore rump.sysctl -w net.inet.arp.keep=$keep

	# Setup an interface
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet $IP4SRC/24
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 rump.ifconfig -w 10

	# Sanity check
	$DEBUG && rump.ifconfig shmif0
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -n $IP4SRC
	atf_check -s not-exit:0 -e ignore rump.arp -n $IP4DST
}

test_cache_expiration()
{
	local arp_keep=$1
	local bonus=2

	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server $arp_keep

	#
	# Check if a cache is expired expectedly
	#
	export RUMP_SERVER=$SOCKSRC
	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP4DST

	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -n $IP4SRC
	# Should be cached
	atf_check -s exit:0 -o ignore rump.arp -n $IP4DST

	atf_check -s exit:0 sleep $(($arp_keep + $bonus))

	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -n $IP4SRC
	# Should be expired
	atf_check -s not-exit:0 -e ignore rump.arp -n $IP4DST
}

cache_expiration_5s_body()
{
	test_cache_expiration 5
}

cache_expiration_10s_body()
{
	test_cache_expiration 10
}

command_body()
{
	local arp_keep=5
	local bonus=2

	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server $arp_keep

	export RUMP_SERVER=$SOCKSRC

	# Add and delete a static entry
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -s 10.0.1.10 b2:a0:20:00:00:10
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:10' rump.arp -n 10.0.1.10
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.10
	atf_check -s exit:0 -o ignore rump.arp -d 10.0.1.10
	$DEBUG && rump.arp -n -a
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.10

	# Add multiple entries via a file
	cat - > ./list <<-EOF
	10.0.1.11 b2:a0:20:00:00:11
	10.0.1.12 b2:a0:20:00:00:12
	10.0.1.13 b2:a0:20:00:00:13
	10.0.1.14 b2:a0:20:00:00:14
	10.0.1.15 b2:a0:20:00:00:15
	EOF
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -f ./list
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:11' rump.arp -n 10.0.1.11
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.11
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:12' rump.arp -n 10.0.1.12
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.12
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:13' rump.arp -n 10.0.1.13
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.13
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:14' rump.arp -n 10.0.1.14
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.14
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:15' rump.arp -n 10.0.1.15
	atf_check -s exit:0 -o match:'permanent' rump.arp -n 10.0.1.15

	# Test arp -a
	atf_check -s exit:0 -o match:'10.0.1.11' rump.arp -n -a
	atf_check -s exit:0 -o match:'10.0.1.12' rump.arp -n -a
	atf_check -s exit:0 -o match:'10.0.1.13' rump.arp -n -a
	atf_check -s exit:0 -o match:'10.0.1.14' rump.arp -n -a
	atf_check -s exit:0 -o match:'10.0.1.15' rump.arp -n -a

	# Flush all entries
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -d -a
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.11
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.12
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.13
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.14
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.15
	atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.1

	# Test temp option
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o ignore rump.arp -s 10.0.1.10 b2:a0:20:00:00:10 temp
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:10' rump.arp -n 10.0.1.10
	atf_check -s exit:0 -o not-match:'permanent' rump.arp -n 10.0.1.10

	# Hm? the cache doesn't expire...
	atf_check -s exit:0 sleep $(($arp_keep + $bonus))
	$DEBUG && rump.arp -n -a
	#atf_check -s not-exit:0 -e ignore rump.arp -n 10.0.1.10

	return 0
}

make_pkt_str()
{
	local target=$1
	local sender=$2
	pkt="> ff:ff:ff:ff:ff:ff, ethertype ARP (0x0806), length 42:"
	pkt="$pkt Request who-has $target tell $sender, length 28"
	echo $pkt
}

garp_body()
{
	local pkt=

	atf_check -s exit:0 ${inetserver} $SOCKSRC
	export RUMP_SERVER=$SOCKSRC

	# Setup an interface
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet 10.0.0.1/24
	atf_check -s exit:0 rump.ifconfig shmif0 inet 10.0.0.2/24 alias
	atf_check -s exit:0 rump.ifconfig shmif0 up
	$DEBUG && rump.ifconfig shmif0

	atf_check -s exit:0 sleep 1
	shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r - > ./out

	# A GARP packet is sent for the primary address
	pkt=$(make_pkt_str 10.0.0.1 10.0.0.1)
	atf_check -s exit:0 -x "cat ./out |grep -q '$pkt'"
	# No GARP packet is sent for the alias address
	pkt=$(make_pkt_str 10.0.0.2 10.0.0.2)
	atf_check -s not-exit:0 -x "cat ./out |grep -q '$pkt'"

	atf_check -s exit:0 rump.ifconfig -w 10
	atf_check -s exit:0 rump.ifconfig shmif0 inet 10.0.0.3/24
	atf_check -s exit:0 rump.ifconfig shmif0 inet 10.0.0.4/24 alias

	# No GARP packets are sent during IFF_UP
	shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r - > ./out
	pkt=$(make_pkt_str 10.0.0.3 10.0.0.3)
	atf_check -s not-exit:0 -x "cat ./out |grep -q '$pkt'"
	pkt=$(make_pkt_str 10.0.0.4 10.0.0.4)
	atf_check -s not-exit:0 -x "cat ./out |grep -q '$pkt'"
}

cache_overwriting_body()
{
	local arp_keep=5
	local bonus=2

	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server $arp_keep

	export RUMP_SERVER=$SOCKSRC

	# Cannot overwrite a permanent cache
	atf_check -s not-exit:0 -e ignore rump.arp -s $IP4SRC b2:a0:20:00:00:ff
	$DEBUG && rump.arp -n -a

	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP4DST
	$DEBUG && rump.arp -n -a
	# Can overwrite a dynamic cache
	atf_check -s exit:0 -o ignore rump.arp -s $IP4DST b2:a0:20:00:00:00
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:00' rump.arp -n $IP4DST
	atf_check -s exit:0 -o match:'permanent' rump.arp -n $IP4DST

	atf_check -s exit:0 -o ignore rump.arp -s 10.0.1.10 b2:a0:20:00:00:10 temp
	$DEBUG && rump.arp -n -a
	atf_check -s exit:0 -o match:'b2:a0:20:00:00:10' rump.arp -n 10.0.1.10
	atf_check -s exit:0 -o not-match:'permanent' rump.arp -n 10.0.1.10
	# Cannot overwrite a temp cache
	atf_check -s not-exit:0 -e ignore rump.arp -s 10.0.1.10 b2:a0:20:00:00:ff
	$DEBUG && rump.arp -n -a

	return 0
}

cleanup()
{
	env RUMP_SERVER=$SOCKSRC rump.halt
	env RUMP_SERVER=$SOCKDST rump.halt
}

dump_src()
{
	export RUMP_SERVER=$SOCKSRC
	rump.netstat -nr
	rump.arp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump_dst()
{
	export RUMP_SERVER=$SOCKDST
	rump.netstat -nr
	rump.arp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump()
{
	dump_src
	dump_dst
	shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r -
}

cache_expiration_5s_cleanup()
{
	$DEBUG && dump
	cleanup
}

cache_expiration_10s_cleanup()
{
	$DEBUG && dump
	cleanup
}

command_cleanup()
{
	$DEBUG && dump
	cleanup
}

garp_cleanup()
{
	$DEBUG && dump_src
	$DEBUG && shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r -
	env RUMP_SERVER=$SOCKSRC rump.halt
}

cache_overwriting_cleanup()
{
	$DEBUG && dump
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case cache_expiration_5s
	atf_add_test_case cache_expiration_10s
	atf_add_test_case command
	atf_add_test_case garp
	atf_add_test_case cache_overwriting
}
