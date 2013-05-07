#!/bin/sh
#
# Script to collect boot-time programs, install them in the given
# dir (to boot from), and
# for the modules:
#	. give them filenames so that the sorted names is the boot order,
#	  and gzip them there
# for the kernel:
#	. just copy it

if [ $# -lt 4 ]
then	echo "Usage: $0 <installdir> <installcmd> <stripcmd> <programlist ...>"
	exit 1
fi

installdir="$1"

if [ ! -d "$installdir" ]
then	echo "$0: expecting directory as first argument, $installdir"
	exit 1
fi

shift

installcmd="$1"
shift

stripcmd="$1"
shift

rm $installdir/*.gz

echo "install cmd: $installcmd"

n=0

while [ $# -gt 0 ]
do	n="`expr $n + 1`"
	if [ $n -ge 10 ]
	then	prefix=mod
	else	prefix=mod0
	fi
	file="$1"
	if [ ! -f $file ]
	then	echo "$0: expecting filenames, $file"
		exit 1
	fi
	basename="`basename $file`"
	if [ $basename != kernel ]
	then	basename="${prefix}${n}_$basename"
	fi
	$installcmd $file $installdir/$basename
	$stripcmd $installdir/$basename
	shift
done

gzip $installdir/mod*
