#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.3 2010/12/03 21:38:49 plunky Exp $

# Copy the FreeBSD src/lib/libelf directory contents to dist.  Run
# this script and you're done.
#
# lib/ is built as SUBDIR from lib/Makefile.
#
# Use the following template to import
#  cvs import src/external/bsd/libelf/dist FreeBSD FreeBSD-X-Y-Z
#
# don't forget to bump the lib/shlib_version if necessary
#

set -e

echo "Adding RCS tags .."
for f in $(grep -RL '\$NetBSD.*\$' dist | grep -v CVS); do
    case $f in
    *.[ch] | *.m4)
	cat - > ${f}_tmp <<- EOF
		/*	\$NetBSD\$	*/

	EOF
	sed -e 's,^__FBSDID.*,\/\* & \*\/,g' ${f} >> ${f}_tmp
	mv ${f}_tmp ${f}
	;;
    *.[0-9])
	cat - ${f} > ${f}_tmp <<- EOF
		.\"	\$NetBSD\$
		.\"
	EOF
	mv ${f}_tmp ${f}
	;;
    *)
	echo "No RCS tag added to ${f}"
	;;
    esac
done

echo "prepare-import done"
