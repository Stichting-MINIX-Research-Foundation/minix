#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Fetch sources if not available
if [ ! -d gmake ];
then
	if [ ! -f make-3.80.tar.bz2 ];
	then
		wget  ftp://ftp.gnu.org/gnu/make/make-3.80.tar.bz2
	fi

	tar -xf make-3.80.tar.bz2 && \
	mv make-3.80 gmake
fi

