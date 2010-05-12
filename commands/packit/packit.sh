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

# can we execute bunzip2?
if bunzip2 --help 2>&1 | grep usage >/dev/null
then	BUNZIP2=bunzip2
else	BUNZIP2=smallbunzip2
fi

cat $f | $BUNZIP2 | pax -r -p e
if [ -f $PI ]
then
	sh -e $PI
	rm -f $PI
fi

for d in /usr/man /usr/local/man /usr/gnu/man /usr/X11R6/man
do	if [ -d "$d" ]
	then makewhatis $d
	fi
done
