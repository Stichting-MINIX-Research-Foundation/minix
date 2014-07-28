#!/bin/sh
# Execute and report the KYUA tests in TAP format

# Execute only if kyua is available
if which kyua 2>&1 > /dev/null
then
	cd /usr/tests

	kyua test 2>&1 > /dev/null

	# If some test fails, kyua return status is 1, which is why we cannot
	# activate this before.
	set -e

	# By default kyua omits successful tests.
	kyua report-tap --results-filter=passed,skipped,xfail,broken,failed

	exit 0
fi

exit 1
