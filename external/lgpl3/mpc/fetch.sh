#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/pkgsrc/distfiles/minix/3.4.0/mpc-1.0.1.tar.gz"
BACKUP_URL="http://www.multiprecision.org/mpc/download/mpc-1.0.1.tar.gz"
FETCH=ftp
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f mpc-1.0.1.tar.gz ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -oxzf mpc-1.0.1.tar.gz
	mv mpc-1.0.1 dist
fi

