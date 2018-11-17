#!/bin/sh

name=$1
pid=$2

kill $pid
set -- .
while kill -0 $pid 2>/dev/null
do
    set -- "$@" "."
    if [ $# -gt 4 ]
    then
        kill kill -9 $pid 2> /dev/null
        break
    fi
    sleep 1
done

set -- .
while kill -0 $pid 2>/dev/null
do
    set -- "$@" "."
    if [ $# -gt 4 ]; then exit 1; fi
    sleep 1
done

exit 0
