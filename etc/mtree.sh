#!/bin/sh
set -e

if [ $# -ne 5 ]
then	echo "Usage: $0 mtreefile TOOL_AWK TOOL_STAT DEST_DIR UNPRIV"
	exit 1
fi

AWK=$2
STAT=$3
DESTDIR=$4
UNPRIV=$5
cat "$1" | while read line
do
	NF="`echo $line | ${AWK} '{ print NF }'`"
	if [ $NF = 4 ]
	then	mode="`echo $line | ${AWK} '{ print $1 }'`"
		owner="`echo $line | ${AWK} '{ print $2 }'`"
		group="`echo $line | ${AWK} '{ print $3 }'`"
		dir="${DESTDIR}`echo $line | ${AWK} '{ print $4 }'`"
		mkdir -p $dir
		targetdev="`${STAT} -f %d $dir/.`"
		if [ $targetdev -lt 256 ]
		then	echo "skipping non-dev $dir properties"
		elif [ $UNPRIV != yes]
		then	chown $owner $dir
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
