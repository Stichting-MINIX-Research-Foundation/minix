#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.2 2010/02/20 02:55:53 joerg Exp $
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

rm -rf build contrib doc examples
rm INSTALL Makefile.am Makefile.in aclocal.m4 config.h.in
rm configure configure.ac CMakeLists.txt */CMakeLists.txt */config_freebsd.h

