#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://www.minix3.org/distfiles-minix/gcc-4.5.3.tar.bz2"
BACKUP_URL="ftp://ftp.gwdg.de/pub/misc/gcc/releases/gcc-4.5.3/gcc-4.5.3.tar.bz2"
FETCH=wget
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Remove a few directories from the start, so we do not end up with a 165MB patch...
DELETEDIRS="include/elf 
	libada libjava libffi libgfortran 
	boehm-gc gnattools 
	gcc/ada gcc/fortran gcc/java 
	gcc/testsuite/ada  gcc/testsuite/gnat gcc/testsuite/gnat.dg 
	gcc/testsuite/gfortran.dg  gcc/testsuite/gfortran.fortran-torture 
"
# Fetch sources if not available
if [ ! -d dist ];
then
	if [ ! -f gcc-4.5.3.tar.bz2 ];
	then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -oxjf gcc-4.5.3.tar.bz2 && \
	mv gcc-4.5.3 dist && \
	cd dist && \
	rm -rf $DELETEDIRS && \
	cat ../patches/* | patch -p1 && \
	cp ../files/minix.h gcc/config/ && \
	cp ../files/t-minix gcc/config/ && \
	cp ../files/minix-spec.h gcc/config/ && \
	cp ../files/arm-minix.h gcc/config/arm/minix.h && \
	cp ../files/i386-minix.h gcc/config/i386/minix.h && \
	cp ../files/gcov-minix-fs-wrapper.h gcc/ 
fi

