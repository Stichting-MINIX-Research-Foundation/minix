#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Fetch sources if not available
if [ ! -d dist ];
then
	if [ ! -f gcc-4.4.3.tar.bz2 ];
	then
		wget  ftp://ftp.gwdg.de/pub/misc/gcc/releases/gcc-4.4.3/gcc-4.4.3.tar.bz2
	fi

	tar -xf gcc-4.4.3.tar.bz2 && \
	mv gcc-4.4.3 dist && \
	cd dist && \
	cat ../../../../tools/gcc/patches/* | patch -p0 && \
	cp ../../../../tools/gcc/files/minix.h gcc/config/ && \
	cp ../../../../tools/gcc/files/minix-spec.h gcc/config/ && \
	cp ../../../../tools/gcc/files/i386-minix.h gcc/config/i386/minix.h && \
	cp ../../../../tools/gcc/files/gcov-minix-fs-wrapper.h gcc/ 
fi

