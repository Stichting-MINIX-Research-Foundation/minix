#!/bin/sh

PI=.postinstall
TMP=/usr/tmp
PATH=/bin:/usr/bin:/usr/local/bin

if [ "$#" -ne 1  ]
then
	echo "Usage: $0 <package file>"
	exit 1
fi

dir=`pwd`
if [ "$1" = "-" ]
then f=""
else	case "$1" in
	/*) f="$1" ;;
	*) f="$dir/$1" ;;
	esac
fi

set -e
cd $TMP
rm -f $PI

if [ -f $PI ]
then	echo "$PI is in $TMP, please remove it first."
	exit 1
fi

if [ ! -f $f ]
then	echo "Couldn't find package $f."
	exit 1
fi

cat $f | smallbunzip2 | pax -r -p e
if [ -f $PI ]
then
	sh -e $PI
	rm -f $PI
fi

makewhatis /usr/man
makewhatis /usr/local/man
makewhatis /usr/gnu/man
