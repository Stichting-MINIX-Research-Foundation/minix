#!/bin/sh

# Idea:
# Iterate over all the /proc/service entries, and
#	for each restatability policy call the policy test function if it is
#	supported. No accounting of failed / successful test is done, as a
#	failed test can currently provoque cascading effects, so instead we
#	fail the test as a whole on the first failurei found.
#
# Supported policies have to be in the POLICIES variable, and define a test
# function.
#
# Known limitations:
#	 - Currently not all recovery policies are tested
#	 - Running this test under X11 hangs the X server
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
#	POL_RESTART,	/* transparent	| preserved	*/
#	POL_LIVE_UPDATE	/* transparent	| preserved	*/

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
		test -f $1 && break
	done
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

	service refresh ${label}
	wait_for_service ${service}

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

	service=$1
	label=$2

	endpoint_pre=$(get_value endpoint ${service})

	service refresh ${label}
	wait_for_service ${service}

	endpoint_post=$(get_value endpoint ${service})

	# This policy doesn't guarantee the endpoint to be kept, but there
	# is a slight chance that it will actualy stay the same, and fail
	# the test.
	if [ ! ${endpoint_post} -eq ${endpoint_pre} ]
	then
		echo ok
	else
		echo not ok
	fi
}

#######################################################################
# main()
#######################################################################
main() {
	local labels service_policies X11

	# If there is a running X server, skip the input driver
	if ps -ef | grep -v grep | grep -q /usr/X11R7/bin/X
	then
		echo "This test can't be run while a Xserver is running"
		echo "not ok # A Xserver is running"
		exit 1
	fi

	labels=$(echo /proc/service/*)
	for label in ${labels}
	do
		service_policies=$(grep policies ${label}|cut -d: -f2)
		for pol in ${service_policies}
		do
			# Check if the supported policy is under test
			if echo "${POLICIES}" | grep -q ${pol}
			then
				echo "# testing ${label} :: ${pol}"
				result=$(pol_${pol} ${label} $(basename ${label}))
				#pol_${pol} ${label} $(basename ${label})
				#result="FAILED"
				if [ "x${result}" != "xok" ]
				then
					echo "not ok # failed ${label}, ${pol}"
					exit 1
				fi
			fi
		done
	done

	echo ok
	exit 0
}

main
