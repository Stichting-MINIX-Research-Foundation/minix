#!/bin/sh
#
# whatis/apropos 1.3 - search whatis(5) database for commands
#							Author: Kees J. Bot
# BUGS
#	whatis file must be as if created by makewhatis(8).
#
# This version includes a fix by Michael Haardt originally posted to 
# comp.os.minix in July 1999.  Fixes for grep provided by Michael in May
# 1999 caused whatis to break, this is now fixed.   (ASW 2004-12-12)

all='exit 0'

case "$1" in
-a)	all="found='exit 0'"
	shift
esac

case $#:$0 in
1:*whatis)
	;;
1:*apropos)
	all="found='exit 0'"
	;;
*)	echo "Usage: `basename $0` [-a] <keyword>" >&2
	exit 1
esac

IFS=":$IFS"
MANPATH="${MANPATH-/usr/local/man:/usr/man}"

found=

for m in $MANPATH
do
	for w in $m/whatis
	do
		test -f $w || continue

		case $0 in
		*whatis)
			grep '^\('$1'\|[^(]* '$1'\)[ ,][^(]*(' $w && eval $all
			;;
		*apropos)
			grep -i "$1" $w && eval $all
		esac
	done
done

$found

echo "`basename $0`: $1: not found" >&2
exit 1
