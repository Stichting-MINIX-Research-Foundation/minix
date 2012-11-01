#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/distfiles-minix/mpc-0.9.tar.gz"
BACKUP_URL="http://www.multiprecision.org/mpc/download/mpc-0.9.tar.gz"
FETCH=wget
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f mpc-0.9.tar.gz ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -oxzf mpc-0.9.tar.gz
	mv mpc-0.9 dist
fi

