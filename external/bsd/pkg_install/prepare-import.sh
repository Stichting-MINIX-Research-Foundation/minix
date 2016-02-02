#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.4 2013/04/20 15:30:34 wiz Exp $
#
# Copy new pkgsrc/pkgtools/pkg_install/files to dist.
# Run this script and check for additional files and
# directories to prune, only relevant content is included.

set -e

cd dist
rm -f Makefile.in README config* install-sh tkpkg
rm -f */Makefile.in */*.cat*
rm -rf CVS */CVS view
