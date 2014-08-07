#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/pkgsrc/distfiles/minix/3.3.0/binutils-2.23.2.tar.bz2"
BACKUP_URL="http://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.bz2"
FETCH=ftp
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d dist ];
then
	if [ ! -f binutils-2.23.2.tar.bz2 ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -oxjf binutils-2.23.2.tar.bz2 && \
	mv binutils-2.23.2 dist && \
	cd dist && \
	cat ../patches/* | patch -p1 && \
	cp ../files/yyscript.h gold && \
	cp ../files/yyscript.c gold && \
	rm -f ld/configdoc.texi
fi

