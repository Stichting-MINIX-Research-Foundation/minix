#!/bin/sh

if [ $# -ne 1 ]
then	echo "Usage: $0 <executable>"
	exit 1
fi

if file $1 | grep NSYM >/dev/null 2>&1; then
  NM="gnm --radix=d"
else
  NM="nm -d"
fi

$NM -n $1 | grep ' [bBdD] [^.]' | awk '{ if (lastpos) printf "%10ld kB  %s\n", ($1-lastpos)/1024, lastname; lastpos=$1; lastname=$3 }' | sort -n
