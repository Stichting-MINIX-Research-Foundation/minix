#!/bin/sh

if [ $# -ne 1 ]
then	echo "Usage: $0 <executable>"
	exit 1
fi

nm -d -n $1 | grep ' [bBdD] ' | awk '{  printf "%10ld kB  %s\n", ($1-lastpos)/1024, lastname; lastpos=$1; lastname=$3 }' | sort -n
