#!/bin/sh
#
# cd 1.3 - equivalents for normally builtin commands.	Author: Kees J. Bot

case $0 in
*/*)	command="`expr "$0" : '.*/\(.*\)'`"
	;;
*)	command="$0"
esac

"$command" "$@"
