#!/bin/sh
export CC=cc
export MINIX=1
if perl -e 'exit 0'
then	perl -w Run
else	echo "perl not installed. please install perl to run unixbench." >&2
	exit 1
fi
