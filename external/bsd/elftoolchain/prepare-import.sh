#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.1 2014/03/09 16:58:03 christos Exp $

# Copy the FreeBSD src/lib/elftwoolchain directory contents to dist.  Run
# this script and you're done. This does not add NetBSD RCSID's just cleans
# existing ones to avoid conflicts in the future
#
# lib/ is built as SUBDIR from lib/Makefile.
#
# Use the following template to import
#  cvs import src/external/bsd/elftoolchain/dist FreeBSD FreeBSD-X-Y-Z
#
# don't forget to bump the lib/shlib_version if necessary
#

set -e

if [ -z "$1" ]
then
	echo "$0: <distdir>" 1>&2 
	exit 1
fi
cleantags $1
