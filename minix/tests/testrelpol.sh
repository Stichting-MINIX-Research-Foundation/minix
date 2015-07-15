#!/bin/sh

# Idea:
# Iterate over all the /proc/service entries, and
#	for each restatability policy call the policy test function if it is
#	supported. No accounting of failed / successful test is done, as a
#	failed test can currently provoque cascading effects, so instead we
#	fail the test as a whole on the first failure found. Live update tests
#	are currently policy-agnostic.
#
# If arguments are given, use this instead of all entries found in
# /proc/service. Full path have to be provided on the command line, like
#   /usr/tests/minix/testrelpol /proc/service/vfs
# to test vfs recovery only.
#
# Supported policies have to be in the POLICIES variable, and define a test
# function.
#
# Known limitations:
#	 - Currently not all recovery policies are tested
#	 - Running this test under X11 hangs the X server
#	 - Live update tests do not test rollback situations
#
# To add a new policy, you have to do the following:
#	1. Add the policy into the active policies array by:
#	POLICIES="${POLICIES} <policyname>"
#
#	2. define the following shell function:
#	pol_<policyname>() {}
#	 - it will recieve the following parameters:
#	   + service filename as $1	: the full path to the proc entry
#	   + label as $2		: the service label
#	 - which prints 'ok' on success, 'not ok' on failure.

# Currently known policies:
# 			/*	user	| endpoint	*/
#	POL_RESET,	/* visible	|  change	*/
#	POL_RESTART	/* transparent	| preserved	*/

#######################################################################
# Utility functions & global state initializations
#######################################################################
POLICIES=""
MAX_RETRY=7 # so that a single test takes at most 10 seconds

# get_value(key, filename)
get_value() {
	if test -f $2
	then
		grep $1 $2 | cut -d: -f2
	else
		echo "Error: service $2 down"
	fi
}

# wait_for_service(filename)
wait_for_service() {
	local retry
	retry=0

	# Arbitrary timeout, found by counting the number of mice crossing
	# the hallway.
	sleep 2
	while test ${retry} -lt ${MAX_RETRY}
	do
		sleep 1
		retry=$((${retry} + 1))
		test -f $1 && test $(get_value restarts $1) -ne $2 && break
	done
}

#######################################################################
# Service management routines
#######################################################################
prepare_service() {
	local label service

	service=$1
	label=$2

	flags=$(get_value flags ${service})
	echo $flags | grep -q 'r' || return 0
	echo $flags | grep -q 'R' && return 0

	service clone $label
	return 1
}

cleanup_service() {
	local label

	label=$1

	service unclone $label
}

#######################################################################
# POLICY: restart
#######################################################################
POLICIES="${POLICIES} restart"
pol_restart() {
	local label service
	local endpoint_pre endpoint_post
	local restarts_pre restarts_post

	service=$1
	label=$2

	restarts_pre=$(get_value restarts ${service})
	endpoint_pre=$(get_value endpoint ${service})

	service fi ${label}
	wait_for_service ${service} ${restarts_pre}

	restarts_post=$(get_value restarts ${service})
	endpoint_post=$(get_value endpoint ${service})

	if [ ${restarts_post} -gt ${restarts_pre} \
		-a ${endpoint_post} -eq ${endpoint_pre} ]
	then
		echo ok
	else
		echo not ok
	fi
}

#######################################################################
# POLICY: reset
#######################################################################
POLICIES="${POLICIES} reset"
pol_reset() {
	local label service
	local endpoint_pre endpoint_post
	local restarts_pre restarts_post

	service=$1
	label=$2

	restarts_pre=$(get_value restarts ${service})
	endpoint_pre=$(get_value endpoint ${service})

	service fi ${label}
	wait_for_service ${service} ${restarts_pre}

	restarts_post=$(get_value restarts ${service})
	endpoint_post=$(get_value endpoint ${service})

	# This policy doesn't guarantee the endpoint to be kept, but there
	# is a slight chance that it will actualy stay the same, and fail
	# the test.
	if [ ${restarts_post} -gt ${restarts_pre} \
		-a ${endpoint_post} -ne ${endpoint_pre} ]
	then
		echo ok
	else
		echo not ok
	fi
}

#######################################################################
# Live update tests
#######################################################################
lu_test_one() {
	local label=$1
	local prog=$2
	local result=$3
	lu_opts=${lu_opts:-}
	lu_maxtime=${lu_maxtime:-3HZ}
	lu_state=${lu_state:-1}

	service ${lu_opts} update ${prog} -label ${label} -maxtime ${lu_maxtime} -state ${lu_state}
	if [ $? -ne $result ]
	then
		return 1
	else
		return 0
	fi
}

lu_test() {
	local label service
	local endpoint_pre endpoint_post
	local restarts_pre restarts_post

	service=$1
	label=$2

	restarts_pre=$(get_value restarts ${service})
	endpoint_pre=$(get_value endpoint ${service})

	lu_test_one ${label} self 0 || return

	# Test live update "prepare only"
	if ! echo "pm rs vfs vm" | grep -q ${label}
	then
		lu_opts="-o" lu_test_one ${label} self 0 || return
	fi

	# Test live update initialization crash
	lu_opts="-x" lu_test_one ${label} self 200 || return

	# Test live update initialization failure
	if ! echo "rs" | grep -q ${label}
	then
		lu_opts="-y" lu_test_one ${label} self 78 || return
	fi

	# Test live update initialization timeout
	if ! echo "rs" | grep -q ${label}
	then
		lu_maxtime="1HZ" lu_opts="-z" lu_test_one ${label} self 4 || return
	fi

	# Test live update from SEF_LU_STATE_EVAL state
	lu_maxtime="1HZ" lu_state="5" lu_test_one ${label} self 4 || return

	restarts_post=$(get_value restarts ${service})
	endpoint_post=$(get_value endpoint ${service})

	# Make sure endpoint and restarts are preserved
	if [ ${restarts_post} -eq ${restarts_pre} \
		-a ${endpoint_post} -eq ${endpoint_pre} ]
	then
		echo ok
	else
		echo not ok
	fi
}

multi_lu_test_one() {
	local result=$1
	shift
	local labels="$*"
	local ret=0
	local index=0
	local once_index=2

	lu_opts=${lu_opts:-}
	lu_maxtime=${lu_maxtime:-3HZ}
	lu_state=${lu_state:-1}
	lu_opts_once=${lu_opts_once:-$lu_opts}
	lu_maxtime_once=${lu_maxtime_once:-$lu_maxtime}
	lu_state_once=${lu_state_once:-$lu_state}

	for label in ${labels}
	do
		index=`expr $index + 1`

		if [ $index -eq $once_index ]
		then
			service ${lu_opts_once} -q update self -label ${label} -maxtime ${lu_maxtime_once} -state ${lu_state_once} || ret=1
		else
			service ${lu_opts} -q update self -label ${label} -maxtime ${lu_maxtime} -state ${lu_state} || ret=1
		fi
	done
	service sysctl upd_run
	if [ $? -ne $result ]
	then
		ret=1
	fi
	if [ $ret -eq 1 ]
	then
		echo not ok
	fi
	return $ret
}

multi_lu_test() {
	local labels="$*"

	multi_lu_test_one 0 ${labels} || return
	lu_opts_once="-x" multi_lu_test_one 200 ${labels} || return
	lu_opts_once="-y" multi_lu_test_one 200 ${labels} || return
	lu_maxtime_once="1HZ" lu_opts_once="-z" multi_lu_test_one 200 ${labels} || return
	lu_maxtime_once="1HZ" lu_state_once="5" multi_lu_test_one 4 ${labels} || return

	echo ok
}

#######################################################################
# main()
#######################################################################
main() {
	local labels service_policies X11

	# If there is a running X server, skip the input driver
	if ps -ef | grep -v grep | grep -q /usr/X11R7/bin/X
	then
		echo "# This test can't be run while a Xserver is running"
		echo "not ok # A Xserver is running"
		exit 1
	fi

	if [ $# -eq 0 ]
	then
		services=$(echo /proc/service/*)
	else
		services="$@"
	fi

	for service in ${services}
	do
		label=$(basename ${service})
		service_policies=$(grep policies ${service}|cut -d: -f2)
		for pol in ${service_policies}
		do
			# Check if the supported policy is under test
			if echo "${POLICIES}" | grep -q ${pol}
			then
				echo "# testing ${label} :: ${pol}"
				cleanup=0
				prepare_service ${service} ${label} || cleanup=1
				result=$(pol_${pol} ${service} ${label})
				if [ "x${result}" != "xok" ]
				then
					echo "not ok # failed ${label}, ${pol}"
					exit 1
				fi
				if [ $cleanup -eq 1 ]
				then
					cleanup_service ${label}
				fi
			fi
		done
	done
	if [ $# -gt 0 ]
	then
		echo "ok # partial test for $@ successful"
		exit 0
	fi

	multi_lu_labels=""
	for service in ${services}
	do
		label=$(basename ${service})
		service_policies=$(grep policies ${service}|cut -d: -f2)
		if echo "${service_policies}" | grep -q "[a-zA-Z]"
		then
			echo "# testing ${label} :: live update+rollback"
			result=$(lu_test ${service} ${label})
			if [ "x${result}" != "xok" ]
			then
				echo "not ok # failed ${label}, live update+rollback"
				exit 1
			fi
			if [ "x${label}" = "xrs" ]
			then
				continue
			fi
			service_flags=$(get_value flags ${service})
			if echo $service_flags | grep -q 's'
			then
				multi_lu_labels="${multi_lu_labels} ${label}"
			fi
		fi
	done
	multi_lu_labels="${multi_lu_labels} rs"
	echo "# testing ${multi_lu_labels} :: whole-OS live update+rollback"
	result=$(multi_lu_test $multi_lu_labels)
	if [ "x${result}" != "xok" ]
	then
		echo "not ok # failed whole-OS live update+rollback"
		exit 1
	fi

	echo ok
	exit 0
}

main "$@"
