#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.3 2012/05/28 17:28:53 jdc Exp $

set -e

rm -rf dist tmp
tar xzf xz-5.0.3.tar.gz
mv xz-5.0.3 dist

cd dist
# Binary files derived from distribution files
rm -rf doc/man
# Files under GPL
rm -rf build-aux extra lib m4/[a-s]* m4/[u-z]* src/scripts/xz* Doxyfile.in
# Files not of relevance
rm -rf ABOUT-NLS aclocal.m4 autogen.sh configure COPYING.*GPL* INSTALL.generic
mkdir po.tmp
mv po/*.po po/*.gmo po.tmp/
rm -rf po
mv po.tmp po
rm -rf debug dos windows
rm -rf Makefile* */Makefile* */*/Makefile* */*/*/Makefile*
# Binary files to be encoded
for f in tests/compress_prepared_bcj_sparc tests/compress_prepared_bcj_x86 \
	 tests/files/*.xz; do
	uuencode -m $f $f > $f.base64
	rm $f
done

# Files under GPL/LGPL kept:
# build-aux/* from autoconf
# lib/*
# m4/*

# Changes to config.h
echo Add build-time endian test to include/config.h:
cat << EOE
#include <sys/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#endif
EOE
