#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.3 2017/04/20 13:11:04 joerg Exp $
#
# Extract the new tarball and rename the libarchive-X.Y.Z directory
# to dist.  Run this script and check for additional files and
# directories to prune, only relevant content is included.

set -e

mkdir tmp
cd tmp
../dist/configure --without-xml2 --without-expat
mv config.h ../include/config_netbsd.h
cd ..
rm -rf tmp

cd dist

rm -rf build contrib doc examples test_utils autom4te.cache
rm -f INSTALL Makefile.am Makefile.in aclocal.m4 config.h.in
rm -f configure configure.ac CMakeLists.txt */CMakeLists.txt */*/CMakeLists.txt
rm -f */config_freebsd.h  CTestConfig.cmake .gitattributes .gitignore
rm -f .travis.yml */*/.cvsignore

