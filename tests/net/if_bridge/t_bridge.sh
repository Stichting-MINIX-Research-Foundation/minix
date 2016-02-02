#	$NetBSD: t_bridge.sh,v 1.11 2015/08/07 00:50:12 ozaki-r Exp $
#
# Copyright (c) 2014 The NetBSD Foundation, Inc.
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

inetserver="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpnet_bridge -lrumpnet_shmif"
inet6server="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpnet_netinet6 -lrumpnet_bridge -lrumpnet_shmif"

SOCK1=unix://commsock1
SOCK2=unix://commsock2
SOCK3=unix://commsock3
IP1=10.0.0.1
IP2=10.0.0.2
IP61=fc00::1
IP62=fc00::2
IPBR1=10.0.0.11
IPBR2=10.0.0.12
IP6BR1=fc00::11
IP6BR2=fc00::12

TIMEOUT=5

atf_test_case basic cleanup
atf_test_case basic6 cleanup
atf_test_case rtable cleanup
atf_test_case member_ip cleanup
atf_test_case member_ip6 cleanup

basic_head()
{
	atf_set "descr" "Does simple if_bridge tests"
	atf_set "require.progs" "rump_server"
}

basic6_head()
{
	atf_set "descr" "Does simple if_bridge tests (IPv6)"
	atf_set "require.progs" "rump_server"
}

rtable_head()
{
	atf_set "descr" "Tests route table operations of if_bridge"
	atf_set "require.progs" "rump_server"
}

member_ip_head()
{
	atf_set "descr" "Tests if_bridge with members with an IP address"
	atf_set "require.progs" "rump_server"
}

member_ip6_head()
{
	atf_set "descr" "Tests if_bridge with members with an IP address (IPv6)"
	atf_set "require.progs" "rump_server"
}

setup_endpoint()
{
	sock=${1}
	addr=${2}
	bus=${3}
	mode=${4}

	export RUMP_SERVER=${sock}
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr ${bus}
	if [ $mode = "ipv6" ]; then
		atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${addr}
	else
		atf_check -s exit:0 rump.ifconfig shmif0 inet ${addr} netmask 0xffffff00
	fi

	atf_check -s exit:0 rump.ifconfig shmif0 up
	rump.ifconfig shmif0
}

test_endpoint()
{
	sock=${1}
	addr=${2}
	bus=${3}
	mode=${4}

	export RUMP_SERVER=${sock}
	atf_check -s exit:0 -o match:shmif0 rump.ifconfig
	if [ $mode = "ipv6" ]; then
		atf_check -s exit:0 -o ignore rump.ping6 -n -c 1 -X $TIMEOUT ${addr}
	else
		atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 ${addr}
	fi
}

show_endpoint()
{
	sock=${1}

	export RUMP_SERVER=${sock}
	rump.ifconfig -v shmif0
}

test_setup()
{
	test_endpoint $SOCK1 $IP1 bus1 ipv4
	test_endpoint $SOCK3 $IP2 bus2 ipv4

	export RUMP_SERVER=$SOCK2
	atf_check -s exit:0 -o match:shmif0 rump.ifconfig
	atf_check -s exit:0 -o match:shmif1 rump.ifconfig
}

test_setup6()
{
	test_endpoint $SOCK1 $IP61 bus1 ipv6
	test_endpoint $SOCK3 $IP62 bus2 ipv6

	export RUMP_SERVER=$SOCK2
	atf_check -s exit:0 -o match:shmif0 rump.ifconfig
	atf_check -s exit:0 -o match:shmif1 rump.ifconfig
}

setup_bridge_server()
{
	export RUMP_SERVER=$SOCK2
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 up

	atf_check -s exit:0 rump.ifconfig shmif1 create
	atf_check -s exit:0 rump.ifconfig shmif1 linkstr bus2
	atf_check -s exit:0 rump.ifconfig shmif1 up
}

setup()
{
	atf_check -s exit:0 ${inetserver} $SOCK1
	atf_check -s exit:0 ${inetserver} $SOCK2
	atf_check -s exit:0 ${inetserver} $SOCK3

	setup_endpoint $SOCK1 $IP1 bus1 ipv4
	setup_endpoint $SOCK3 $IP2 bus2 ipv4
	setup_bridge_server
}

setup6()
{
	atf_check -s exit:0 ${inet6server} $SOCK1
	atf_check -s exit:0 ${inet6server} $SOCK2
	atf_check -s exit:0 ${inet6server} $SOCK3

	setup_endpoint $SOCK1 $IP61 bus1 ipv6
	setup_endpoint $SOCK3 $IP62 bus2 ipv6
	setup_bridge_server
}

setup_bridge()
{
	export RUMP_SERVER=$SOCK2
	atf_check -s exit:0 rump.ifconfig bridge0 create
	atf_check -s exit:0 rump.ifconfig bridge0 up

	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 /sbin/brconfig bridge0 add shmif0
	atf_check -s exit:0 /sbin/brconfig bridge0 add shmif1
	/sbin/brconfig bridge0
	unset LD_PRELOAD
	rump.ifconfig shmif0
	rump.ifconfig shmif1
}

setup_member_ip()
{
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 rump.ifconfig shmif0 $IPBR1/24
	atf_check -s exit:0 rump.ifconfig shmif1 $IPBR2/24
	atf_check -s exit:0 rump.ifconfig -w 10
	/sbin/brconfig bridge0
	unset LD_PRELOAD
	rump.ifconfig shmif0
	rump.ifconfig shmif1
}

setup_member_ip6()
{
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $IP6BR1
	atf_check -s exit:0 rump.ifconfig shmif1 inet6 $IP6BR2
	atf_check -s exit:0 rump.ifconfig -w 10
	/sbin/brconfig bridge0
	unset LD_PRELOAD
	rump.ifconfig shmif0
	rump.ifconfig shmif1
}

teardown_bridge()
{
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 /sbin/brconfig bridge0 delete shmif0
	atf_check -s exit:0 /sbin/brconfig bridge0 delete shmif1
	/sbin/brconfig bridge0
	unset LD_PRELOAD
	rump.ifconfig shmif0
	rump.ifconfig shmif1
}

test_setup_bridge()
{
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 -o match:shmif0 /sbin/brconfig bridge0
	atf_check -s exit:0 -o match:shmif1 /sbin/brconfig bridge0
	/sbin/brconfig bridge0
	unset LD_PRELOAD
}

cleanup()
{
	env RUMP_SERVER=$SOCK1 rump.halt
	env RUMP_SERVER=$SOCK2 rump.halt
	env RUMP_SERVER=$SOCK3 rump.halt
}

dump_bus()
{
	/usr/bin/shmif_dumpbus -p - bus1 2>/dev/null| /usr/sbin/tcpdump -n -e -r -
	/usr/bin/shmif_dumpbus -p - bus2 2>/dev/null| /usr/sbin/tcpdump -n -e -r -
}

down_up_interfaces()
{
	export RUMP_SERVER=$SOCK1
	rump.ifconfig shmif0 down
	rump.ifconfig shmif0 up
	export RUMP_SERVER=$SOCK3
	rump.ifconfig shmif0 down
	rump.ifconfig shmif0 up
}

test_ping_failure()
{
	export RUMP_SERVER=$SOCK1
	atf_check -s not-exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IP2
	export RUMP_SERVER=$SOCK3
	atf_check -s not-exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IP1
}

test_ping_success()
{
	export RUMP_SERVER=$SOCK1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IP2
	rump.ifconfig -v shmif0

	export RUMP_SERVER=$SOCK3
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IP1
	rump.ifconfig -v shmif0
}

test_ping6_failure()
{
	export RUMP_SERVER=$SOCK1
	atf_check -s not-exit:0 -o ignore rump.ping6 -q -n -c 1 -X $TIMEOUT $IP62
	export RUMP_SERVER=$SOCK3
	atf_check -s not-exit:0 -o ignore rump.ping6 -q -n -c 1 -X $TIMEOUT $IP61
}

test_ping6_success()
{
	export RUMP_SERVER=$SOCK1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -c 1 -X $TIMEOUT $IP62
	rump.ifconfig -v shmif0

	export RUMP_SERVER=$SOCK3
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -c 1 -X $TIMEOUT $IP61
	rump.ifconfig -v shmif0
}

test_ping_member()
{
	export RUMP_SERVER=$SOCK1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IPBR1
	rump.ifconfig -v shmif0
	# Test for PR#48104
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IPBR2
	rump.ifconfig -v shmif0

	export RUMP_SERVER=$SOCK3
	rump.ifconfig -v shmif0
	# Test for PR#48104
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IPBR1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping -q -n -w $TIMEOUT -c 1 $IPBR2
	rump.ifconfig -v shmif0
}

test_ping6_member()
{
	export RUMP_SERVER=$SOCK1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -X $TIMEOUT -c 1 $IP6BR1
	rump.ifconfig -v shmif0
	# Test for PR#48104
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -X $TIMEOUT -c 1 $IP6BR2
	rump.ifconfig -v shmif0

	export RUMP_SERVER=$SOCK3
	rump.ifconfig -v shmif0
	# Test for PR#48104
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -X $TIMEOUT -c 1 $IP6BR1
	rump.ifconfig -v shmif0
	atf_check -s exit:0 -o ignore rump.ping6 -q -n -X $TIMEOUT -c 1 $IP6BR2
	rump.ifconfig -v shmif0
}

get_number_of_caches()
{
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	echo $(($(/sbin/brconfig bridge0 |grep -A 100 "Address cache" |wc -l) - 1))
	unset LD_PRELOAD
}

test_brconfig_maxaddr()
{
	addr1= addr3= n=

	# Get MAC addresses of the endpoints.
	export RUMP_SERVER=$SOCK1
	addr1=$(rump.ifconfig shmif0 |awk '/address:/ { print $2;}')
	export RUMP_SERVER=$SOCK3
	addr3=$(rump.ifconfig shmif0 |awk '/address:/ { print $2;}')
	unset RUMP_SERVER

	# Refill the MAC addresses of the endpoints.
	export RUMP_SERVER=$SOCK1
	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP2
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr3 shmif1" /sbin/brconfig bridge0

	# Check the default # of caches is 100
	atf_check -s exit:0 -o match:"max cache: 100" /sbin/brconfig bridge0

	# Test two MAC addresses are cached
	n=$(get_number_of_caches)
	atf_check_equal $n 2

	# Limit # of caches to one
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 maxaddr 1
	atf_check -s exit:0 -o match:"max cache: 1" /sbin/brconfig bridge0
	/sbin/brconfig bridge0

	# Test just one address is cached
	n=$(get_number_of_caches)
	atf_check_equal $n 1

	# Increase # of caches to two
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 maxaddr 2
	atf_check -s exit:0 -o match:"max cache: 2" /sbin/brconfig bridge0
	unset LD_PRELOAD

	# Test we can cache two addresses again
	export RUMP_SERVER=$SOCK1
	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP2
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr3 shmif1" /sbin/brconfig bridge0
	unset LD_PRELOAD
}

basic_body()
{
	setup
	test_setup

	# Enable once PR kern/49219 is fixed
	#test_ping_failure

	setup_bridge
	sleep 1
	test_setup_bridge
	test_ping_success

	teardown_bridge
	test_ping_failure
}

basic6_body()
{
	setup6
	test_setup6

	test_ping6_failure

	setup_bridge
	sleep 1
	test_setup_bridge
	test_ping6_success

	teardown_bridge
	test_ping6_failure
}

rtable_body()
{
	addr1= addr3=

	setup
	setup_bridge

	# Get MAC addresses of the endpoints.
	export RUMP_SERVER=$SOCK1
	addr1=$(rump.ifconfig shmif0 |awk '/address:/ { print $2;}')
	export RUMP_SERVER=$SOCK3
	addr3=$(rump.ifconfig shmif0 |awk '/address:/ { print $2;}')
	unset RUMP_SERVER

	# Confirm there is no MAC address caches.
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 -o not-match:"$addr1" /sbin/brconfig bridge0
	atf_check -s exit:0 -o not-match:"$addr3" /sbin/brconfig bridge0
	unset LD_PRELOAD

	# Make the bridge learn the MAC addresses of the endpoints.
	export RUMP_SERVER=$SOCK1
	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP2
	unset RUMP_SERVER

	# Tests the addresses are in the cache.
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr3 shmif1" /sbin/brconfig bridge0

	# Tests brconfig deladdr
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 deladdr "$addr1"
	atf_check -s exit:0 -o not-match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 deladdr "$addr3"
	atf_check -s exit:0 -o not-match:"$addr3 shmif1" /sbin/brconfig bridge0
	unset LD_PRELOAD

	# Refill the MAC addresses of the endpoints.
	export RUMP_SERVER=$SOCK1
	atf_check -s exit:0 -o ignore rump.ping -n -w $TIMEOUT -c 1 $IP2
	unset RUMP_SERVER
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	/sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o match:"$addr3 shmif1" /sbin/brconfig bridge0

	# Tests brconfig flush.
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 flush
	atf_check -s exit:0 -o not-match:"$addr1 shmif0" /sbin/brconfig bridge0
	atf_check -s exit:0 -o not-match:"$addr3 shmif1" /sbin/brconfig bridge0
	unset LD_PRELOAD

	# Tests brconfig timeout.
	export RUMP_SERVER=$SOCK2
	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 -o match:"timeout: 1200" /sbin/brconfig bridge0
	atf_check -s exit:0 -o ignore /sbin/brconfig bridge0 timeout 10
	atf_check -s exit:0 -o match:"timeout: 10" /sbin/brconfig bridge0
	unset LD_PRELOAD

	# Tests brconfig maxaddr.
	test_brconfig_maxaddr

	# TODO: brconfig static/flushall/discover/learn
	# TODO: cache expiration; it takes 5 minutes at least and we want to
	#       wait here so long. Should we have a sysctl to change the period?
}

member_ip_body()
{
	setup
	test_setup

	# Enable once PR kern/49219 is fixed
	#test_ping_failure

	setup_bridge
	sleep 1
	test_setup_bridge
	test_ping_success

	setup_member_ip
	test_ping_member

	teardown_bridge
	test_ping_failure
}

member_ip6_body()
{
	setup6
	test_setup6

	test_ping6_failure

	setup_bridge
	sleep 1
	test_setup_bridge
	test_ping6_success

	setup_member_ip6
	test_ping6_member

	teardown_bridge
	test_ping6_failure
}

basic_cleanup()
{
	dump_bus
	cleanup
}

basic6_cleanup()
{
	dump_bus
	cleanup
}

rtable_cleanup()
{
	dump_bus
	cleanup
}

member_ip_cleanup()
{
	dump_bus
	cleanup
}

member_ip6_cleanup()
{
	dump_bus
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case basic6
	atf_add_test_case rtable
	atf_add_test_case member_ip
	atf_add_test_case member_ip6
}
