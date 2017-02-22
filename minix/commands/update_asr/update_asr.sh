#!/bin/sh
# ASR live update script by David van Moolenbroek <david@minix3.org>

# The path to the initial, standard system service binaries.
SERVICE_PATH=/service

# The path to the alternative, ASR-rerandomized system service binaries.
# The path used here is typically a symlink into /usr for size reasons.
SERVICE_ASR_PATH=$SERVICE_PATH/asr

# A space-separated list of labels not to update in any case.  The list
# includes the memory service, which is currently not compiled with bitcode
# and therefore also not instrumented.  It also contains the VM service,
# for which ASR is possible but too dangerous: much of its address space is
# deliberately ignored by the instrumentation, and ASR would invalidate any
# pointers from the ignored memory to the relocated memory.  Note that
# skipped services may still have rerandomized binaries on disk.
SKIP="memory vm"

# Custom live update states to use for certain label prefixes.  This list is
# made up of space-separated tokens, each token consisting of a label prefix,
# followed by a colon, followed by the state number to use for those labels.
# Currently it contains all services that make use of worker threads.  This
# setting should not need to exist; see the corresponding TODO item below.
STATES="vfs:2 ahci_:2 virtio_blk_:2"

# If this variable is set, it is used as timeout for the live updates.  The
# minix-service(8) argument takes a number of click ticks, or a number of
# seconds if the value ends with "HZ".
TIMEOUT=300HZ

# Configuration ends here.

debug() {
	if [ $verbose -eq 1 ]; then
		echo "$@"
	fi
}

verbose=0
ret=0

while getopts 'v' opt; do
	case $opt in
	v)	verbose=1
		;;
	?)	echo "Usage: $0 [-v] [label [label..]]" >&2
		exit 1
	esac
done
shift $(($OPTIND - 1))

if [ $# -eq 0 ]; then
	services=$(echo /proc/service/*)
else
	services=
	for label in $@; do
		services="$services /proc/service/$label"
	done
fi

for service in $services; do
	label=$(basename $service)
	filename=$(grep filename: $service | cut -d' ' -f2)
	count=$(grep ASRcount: $service | cut -d' ' -f2)

	# Start by making sure we are not supposed to skip this service.
	if echo " $SKIP " | grep -q " $label "; then
		debug "skipping $label: found in skip list"
		continue
	fi

	# The base binary of the program has number 0 and must be present.
	if [ ! -f $SERVICE_PATH/$filename ]; then
		debug "skipping $label: no base binary found"
		continue
	fi

	# Count the ASR binaries for this program, starting from number 1.
	# There must be at least one, so that we can switch between versions.
	# By counting using a number rather than using a directory iterator,
	# we avoid potential problems with gaps between the numbers by
	# stopping at the first number for which no binary is present.
	total=1
	while [ -f $SERVICE_ASR_PATH/$filename-$total ]; do
		total=$(($total + 1))
	done

	if [ $total -eq 1 ]; then
		debug "skipping $label: no ASR binaries found"
		continue
	fi

	# Determine the path name of the binary to use for this update.
	# TODO: pick the next binary at random rather than round-robin.
	count=$((($count + 1) % $total))
	if [ $count -eq 0 ]; then
		binary=$SERVICE_PATH/$filename
	else
		binary=$SERVICE_ASR_PATH/$filename-$count
	fi

	# Check whether the live update should use a state other than the
	# default (namely state 1, which is "work free").  In particular, any
	# programs that use threads typically need another state (namely state
	# 2, which is "request free".  TODO: allow services to specify their
	# own default state, thus avoiding the redundancy introduced here.
	state=
	for token in $STATES; do
		prefix=$(echo $token | cut -d: -f1)
		if echo "$label" | grep -q -e "^$prefix"; then
			state="-state $(echo $token | cut -d: -f2)"
		fi
	done

	# Apply a custom timeout if present.  This may be necessary in VMs.
	maxtime=
	if [ -n "$TIMEOUT" ]; then
		maxtime="-maxtime $TIMEOUT"
	fi

	# Perform the live update.  The update may legitimately fail if the
	# service is not in the right state.  TODO: report transient errors
	# as debugging output only.
	minix-service -a update $binary -progname $filename -label $label \
		-asr-count $count $state $maxtime
	error=$?
	if [ $error -eq 0 ]; then
		debug "updated $label to number $count, total $total"
	else
		echo "failed updating $label: error $error" >&2
		ret=1
	fi
done

exit $ret
