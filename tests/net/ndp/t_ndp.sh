#	$NetBSD: t_ndp.sh,v 1.6 2015/08/18 00:58:35 ozaki-r Exp $
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

inetserver="rump_server -lrumpnet -lrumpnet_net -lrumpnet_netinet"
inetserver="$inetserver -lrumpnet_netinet6 -lrumpnet_shmif"
HIJACKING="env LD_PRELOAD=/usr/lib/librumphijack.so RUMPHIJACK=sysctl=yes"

SOCKSRC=unix://commsock1
SOCKDST=unix://commsock2
IP6SRC=fc00::1
IP6DST=fc00::2

DEBUG=true
TIMEOUT=1

atf_test_case cache_expiration cleanup
atf_test_case command cleanup
atf_test_case cache_overwriting cleanup

cache_expiration_head()
{
	atf_set "descr" "Tests for NDP cache expiration"
	atf_set "require.progs" "rump_server"
}

command_head()
{
	atf_set "descr" "Tests for commands of ndp(8)"
	atf_set "require.progs" "rump_server"
}

cache_overwriting_head()
{
	atf_set "descr" "Tests for behavior of overwriting NDP caches"
	atf_set "require.progs" "rump_server"
}

setup_dst_server()
{
	export RUMP_SERVER=$SOCKDST
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $IP6DST
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 rump.ifconfig -w 10

	$DEBUG && rump.ifconfig shmif0
	$DEBUG && rump.ndp -n -a
}

setup_src_server()
{
	$DEBUG && ulimit -c unlimited
	export RUMP_SERVER=$SOCKSRC

	# Setup an interface
	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 $IP6SRC
	atf_check -s exit:0 rump.ifconfig shmif0 up
	atf_check -s exit:0 rump.ifconfig -w 10

	# Sanity check
	$DEBUG && rump.ifconfig shmif0
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o ignore rump.ndp -n $IP6SRC
	atf_check -s not-exit:0 -o ignore -e ignore rump.ndp -n $IP6DST
}

get_timeout()
{
	local timeout=$(env RUMP_SERVER=$SOCKSRC rump.ndp -n $IP6DST |grep $IP6DST|awk '{print $4;}')
	timeout=${timeout%s}
	echo $timeout
}

cache_expiration_body()
{
	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server

	#
	# Check if a cache is expired expectedly
	#
	export RUMP_SERVER=$SOCKSRC
	atf_check -s exit:0 -o ignore rump.ping6 -n -X $TIMEOUT -c 1 $IP6DST

	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n $IP6SRC
	# Should be cached
	atf_check -s exit:0 -o not-match:'permanent' rump.ndp -n $IP6DST

	timeout=$(get_timeout $IP6DST)

	atf_check -s exit:0 sleep $(($timeout + 1))

	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n $IP6SRC
	# Expired but remains until GC sweaps it (1 day)
	atf_check -s exit:0 -o match:'(1d0h0m|23h59m)' rump.ndp -n $IP6DST
}

ifdown_dst_server()
{
	export RUMP_SERVER=$SOCKDST
	atf_check -s exit:0 rump.ifconfig shmif0 down
	export RUMP_SERVER=$SOCKSRC
}

command_body()
{
	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server

	export RUMP_SERVER=$SOCKSRC

	# Add and delete a static entry
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o ignore rump.ndp -s fc00::10 b2:a0:20:00:00:10
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n fc00::10
	atf_check -s exit:0 -o match:'deleted' rump.ndp -d fc00::10
	$DEBUG && rump.ndp -n -a
	atf_check -s not-exit:0 -o ignore -e ignore rump.ndp -n fc00::10

	# Add multiple entries via a file (XXX not implemented)
	#cat - > ./list <<-EOF
	#fc00::11 b2:a0:20:00:00:11
	#fc00::12 b2:a0:20:00:00:12
	#fc00::13 b2:a0:20:00:00:13
	#fc00::14 b2:a0:20:00:00:14
	#fc00::15 b2:a0:20:00:00:15
	#EOF
	#$DEBUG && rump.ndp -n -a
	#atf_check -s exit:0 -o ignore rump.ndp -f ./list
	#$DEBUG && rump.ndp -n -a

	atf_check -s exit:0 -o ignore rump.ping6 -n -X $TIMEOUT -c 1 $IP6DST
	atf_check -s exit:0 -o ignore rump.ndp -s fc00::11 b2:a0:20:00:00:11
	atf_check -s exit:0 -o ignore rump.ndp -s fc00::12 b2:a0:20:00:00:12

	atf_check -s exit:0 -o not-match:'permanent' rump.ndp -n $IP6DST
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n fc00::11
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n fc00::12

	# Test ndp -a
	atf_check -s exit:0 -o match:'fc00::11' rump.ndp -n -a
	atf_check -s exit:0 -o match:'fc00::12' rump.ndp -n -a

	# Ensure no packet upsets the src server
	ifdown_dst_server

	# Flush all entries (-c)
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o ignore rump.ndp -c
	atf_check -s not-exit:0 -o ignore -e ignore rump.ndp -n $IP6DST
	# Only the static caches are not deleted
	atf_check -s exit:0 -o ignore -e ignore rump.ndp -n fc00::11
	atf_check -s exit:0 -o ignore -e ignore rump.ndp -n fc00::12

	# Test temp option (XXX it doesn't work; expire time isn't set)
	$DEBUG && rump.ndp -n -a
	#atf_check -s exit:0 -o ignore rump.ndp -s fc00::10 b2:a0:20:00:00:10 temp
	rump.ndp -s fc00::10 b2:a0:20:00:00:10 temp
	$DEBUG && rump.ndp -n -a
	#atf_check -s exit:0 -o not-match:'permanent' rump.ndp -n fc00::10

	return 0
}

cache_overwriting_body()
{
	atf_check -s exit:0 ${inetserver} $SOCKSRC
	atf_check -s exit:0 ${inetserver} $SOCKDST

	setup_dst_server
	setup_src_server

	export RUMP_SERVER=$SOCKSRC

	# Cannot overwrite a permanent cache
	atf_check -s not-exit:0 -e ignore rump.ndp -s $IP6SRC b2:a0:20:00:00:ff
	$DEBUG && rump.ndp -n -a

	atf_check -s exit:0 -o ignore rump.ping6 -n -X $TIMEOUT -c 1 $IP6DST
	$DEBUG && rump.ndp -n -a
	# Can overwrite a dynamic cache
	atf_check -s exit:0 -o ignore rump.ndp -s $IP6DST b2:a0:20:00:00:00
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o match:'permanent' rump.ndp -n $IP6DST

	# Test temp option (XXX it doesn't work; expire time isn't set)
	#atf_check -s exit:0 -o ignore rump.ndp -s fc00::10 b2:a0:20:00:00:10 temp
	#$DEBUG && rump.ndp -n -a
	#atf_check -s exit:0 -o not-match:'permanent' rump.ndp -n fc00::10
	# Cannot overwrite a temp cache
	#atf_check -s not-exit:0 -e ignore rump.ndp -s fc00::10 b2:a0:20:00:00:ff
	#$DEBUG && rump.ndp -n -a

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
	rump.ndp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump_dst()
{
	export RUMP_SERVER=$SOCKDST
	rump.netstat -nr
	rump.ndp -n -a
	rump.ifconfig
	$HIJACKING dmesg
}

dump()
{
	dump_src
	dump_dst
	shmif_dumpbus -p - bus1 2>/dev/null| tcpdump -n -e -r -
	$DEBUG && gdb -ex bt /usr/bin/rump_server rump_server.core
	$DEBUG && gdb -ex bt /usr/sbin/rump.ndp rump.ndp.core
}

cache_expiration_cleanup()
{
	$DEBUG && dump
	cleanup
}

command_cleanup()
{
	$DEBUG && dump
	cleanup
}

cache_overwriting_cleanup()
{
	$DEBUG && dump
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case cache_expiration
	atf_add_test_case command
	atf_add_test_case cache_overwriting
}
