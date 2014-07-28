#!/bin/sh
#
#	spell 1.1 - show unknown words			Author: Kees J. Bot
#								28 Apr 1995

dict=words

while getopts 'd:' opt
do
	case $opt in
	d)	dict="$OPTARG"
		;;
	?)	echo "Usage: spell [-d dict] [file ...]" >&2; exit 1
	esac
done
shift `expr $OPTIND - 1`

case "$dict" in
*/*)	;;
*)	dict="/usr/lib/dict/$dict"
esac

{
	if [ $# = 0 ]
	then
		prep
	else
		for file
		do
			prep "$file"
		done
	fi
} | {
	sort -u | comm -23 - "$dict"
}
