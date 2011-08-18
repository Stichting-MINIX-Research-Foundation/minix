#!/bin/sh
set -e

if [ $# -ne 1 ]
then	echo "Usage: $0 mtreefile"
	exit 1
fi

cat "$1" | while read line
do
	NF="`echo $line | awk '{ print NF }'`"
	if [ $NF = 4 ]
	then	mode="`echo $line | awk '{ print $1 }'`"
		owner="`echo $line | awk '{ print $2 }'`"
		group="`echo $line | awk '{ print $3 }'`"
		dir="`echo $line | awk '{ print $4 }'`"
		mkdir -p $dir
		targetdev="`stat -f %d $dir/.`"
		if [ $targetdev -lt 256 ]
		then	echo "skipping non-dev $dir properties"
		else	chown $owner $dir
			chmod $mode $dir
			chgrp $group $dir
		fi
	elif [ $NF = 3 ]
	then	target="`echo $line | awk '{ print $3 }'`"
		linkfile="`echo $line | awk '{ print $1 }'`"
		rm -f $linkfile
		ln -s $target $linkfile
	else	echo odd line.
		exit 1
	fi
done
