# $NetBSD: t_wait.sh,v 1.3 2015/09/30 06:08:36 ozaki-r Exp $
#
# Copyright (c) 2008, 2009, 2010 The NetBSD Foundation, Inc.
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

atf_test_case individual
individual_head() {
	atf_set "descr" "Tests that waiting for individual jobs works"
}
individual_body() {
	# atf-sh confuses wait for some reason; work it around by creating
	# a helper script that executes /bin/sh directly.
	cat >individualhelper.sh <<\EOF
sleep 3 &
sleep 1 &

wait %1
if [ $? -ne 0 ]; then
    echo "Waiting of first job failed"
    exit 1
fi

wait %2
if [ $? -ne 0 ]; then
    echo "Waiting of second job failed"
    exit 1
fi

exit 0
EOF
	output=$(/bin/sh individualhelper.sh)
	[ $? -eq 0 ] || atf_fail "${output}"
	rm -f individualhelper.sh
}

atf_test_case kill
kill_head() {
	atf_set "descr" "Tests that killing the shell while in wait calls trap"
}
kill_body() {
	# atf-sh confuses wait for some reason; work it around by creating
	# a helper script that executes /bin/sh directly.
	local s=$PWD/killhelper.sh
	local z=/tmp/killhelper.$$ 
	local pid=

	cat >$s <<\EOF
#!/bin/sh
trap "echo SIGHUP" 1
sleep 10 &
sl=$!
wait
echo $?
EOF
	chmod +x $s

	$s > $z &
	pid=$!
	sleep 1

	# XXX: built-in kill does not work?
	/bin/kill -HUP $pid
	sleep 1

	output="$(cat $z | tr '\n' ' ')"
	rm -f $s $z
	if [ "$output" != "SIGHUP 129 " ]; then
		atf_fail "${output} != 'SIGHUP 129 '"
	fi
}

atf_init_test_cases() {
	atf_add_test_case individual
	atf_add_test_case kill
}
