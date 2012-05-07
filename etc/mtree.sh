#!/bin/sh
set -e

if [ $# -ne 4 -a $# -ne 5 ]
then	echo "Usage: $0 mtreefile TOOL_AWK TOOL_STAT UNPRIV DESTDIR"
	exit 1
fi

AWK=$2
STAT=$3
UNPRIV=$4

if [ $# -eq 5 ]
then	DESTDIR=$5
else	DESTDIR="/"	# If not set, default to root (i.e., normal case)
fi

cat "$1" | while read line
do
	NF="`echo $line | ${AWK} '{ print NF }'`"
	if [ $NF = 4 ]
	then	mode="`echo $line | ${AWK} '{ print $1 }'`"
		owner="`echo $line | ${AWK} '{ print $2 }'`"
		group="`echo $line | ${AWK} '{ print $3 }'`"
		dir="${DESTDIR}`echo $line | ${AWK} '{ print $4 }'`"
		mkdir -p $dir
		echo $dir
		targetdev="`${STAT} -f %d $dir/.`"
		if [ $targetdev -lt 256 ]
		then	echo "skipping non-dev $dir properties"
		elif [ $UNPRIV != yes ]
		then
				chown $owner $dir
				chmod $mode $dir
				chgrp $group $dir
		fi
	elif [ $NF = 3 ]
	then	target="`echo $line | ${AWK} '{ print $3 }'`"
		linkfile="${DESTDIR}`echo $line | ${AWK} '{ print $1 }'`"
		rm -f $linkfile
		ln -s $target $linkfile
	else	echo odd line.
		exit 1
	fi
done
