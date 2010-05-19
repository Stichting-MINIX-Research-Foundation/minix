#!/bin/sh
#
#	mkdep 1.1 - Generate Makefile dependencies.	Author: Kees J. Bot
#
# Does what 'cc -M' should do, but no compiler gets it right, they all
# strip the leading path of the '.o' file.)
#
# Added option to generate .depend files in subdirectories of given dir.
# 							Jorrit N. Herder

set -e

case $# in

# Display help ...
0)	
	echo "Usage: mkdep 'cpp command' file ..." >&2
	echo "       mkdep directory" >&2
;;

# Create .depend files ...
1)
	echo "Creating .depend files in $1"
	for dir in `find $1 -type d ! -name CVS ! -name .svn`
	do
		touch $dir/.depend
	done

;;


# Get dependencies ... 
*)
	cpp="$1"; shift

	for f
	do
		: < "$f" || exit

		o=`expr "$f" : '\(.*\)\..*'`.o
		o=`basename $o`

		echo

		$cpp "$f" | \
			sed -e '/^#/!d
				s/.*"\(.*\)".*/\1/
				s:^\./::' \
			    -e '/^<built-in>$/d' \
			    -e '/^<command.line>$/d' \
			    -e "s:^:$o\:	:" | \
			sort -u
	done
esac

exit 0

#
# $PchId: mkdep.sh,v 1.3 1998/07/23 21:24:38 philip Exp $
#
