#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

FETCH=ftp
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Configure fetch method - TEXINFO
URL="http://www.minix3.org/pkgsrc/distfiles/minix/3.4.0/texinfo-4.8.tar.bz2"
BACKUP_URL="ftp://ftp.gnu.org/gnu/texinfo/texinfo-4.8.tar.bz2"

# Fetch sources if not available
if [ ! -d texinfo ];
then
	if [ ! -f texinfo-4.8.tar.bz2 ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -xjf texinfo-4.8.tar.bz2 && \
	cd texinfo-4.8 && \
	cat ../../usr.bin/texinfo/patches/* | patch -p1 && \
	cd - && \
	mv texinfo-4.8 texinfo
fi

