#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/distfiles-minix/gcc-4.4.3.tar.bz2"
BACKUP_URL="ftp://ftp.gwdg.de/pub/misc/gcc/releases/gcc-4.4.3/gcc-4.4.3.tar.bz2"
FETCH=wget
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d dist ];
then
	if [ ! -f gcc-4.4.3.tar.bz2 ];
	then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	bsdtar -oxf gcc-4.4.3.tar.bz2 && \
	mv gcc-4.4.3 dist && \
	cd dist && \
	cat ../../../../tools/gcc/patches/* | patch -p0 && \
	cp ../../../../tools/gcc/files/minix.h gcc/config/ && \
	cp ../../../../tools/gcc/files/minix-spec.h gcc/config/ && \
	cp ../../../../tools/gcc/files/i386-minix.h gcc/config/i386/minix.h && \
	cp ../../../../tools/gcc/files/gcov-minix-fs-wrapper.h gcc/ 
fi

