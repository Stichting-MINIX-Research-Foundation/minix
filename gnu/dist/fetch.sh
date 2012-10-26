#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/distfiles-minix/make-3.80.tar.bz2"
BACKUP_URL="ftp://ftp.gnu.org/gnu/make/make-3.80.tar.bz2"
FETCH=wget
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d gmake ];
then
	if [ ! -f make-3.80.tar.bz2 ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -xjf make-3.80.tar.bz2 && \
	mv make-3.80 gmake
fi

