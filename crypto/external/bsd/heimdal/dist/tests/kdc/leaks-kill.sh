#!/bin/sh

name=$1
pid=$2

ec=0

if [ "$(uname -s)" = "Darwin" ] ; then
    echo "leaks check on $name ($pid)"
    leaks $pid > leaks-log 2>&1 || \
        { echo "leaks failed: $?"; cat leaks-log; exit 1; }

    grep "Process $pid: 0 leaks for 0 total leaked bytes" leaks-log > /dev/null || \
	{ echo "Memory leak in $name" ; echo ""; cat leaks-log; ec=1; }

    # [ "$ec" != "0" ] && { env PS1=": leaks-debugger !!!! ; " bash ; }

fi

kill $pid
sleep 3
kill -9 $pid 2> /dev/null

rm -f leaks-log

exit $ec
