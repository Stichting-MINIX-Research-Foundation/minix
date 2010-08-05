#!/bin/sh

# Look at /usr/pkg/bin first in case there is an old nm in /usr/bin
PATH=/usr/pkg/bin:$PATH:/usr/gnu/bin

# Check usage
if [ $# -lt 1 ]
then	echo "Usage: unstack <executable> [0x... [0x... ] ]"
	echo "       datasizes <executable>"
	exit 1
fi

# Check invocation mode
case "`basename $0`" in
	datasizes)
		mode=data
		;;
	unstack)
		mode=stack
		;;
	*)
		echo "Invoked as $0?"
		exit 1
		;;
esac

# Get executable name
executable=$1
shift

# gnu nm can be gnm or nm
if which gnm >/dev/null 2>&1
then	GNM=gnm
else	GNM=nm
fi

# Invoke gnu nm or ack nm?
if file $executable | grep NSYM >/dev/null 2>&1
then	NM="$GNM --radix=d"
else	NM="acknm -d"
fi

# Invoked as unstack?
if [ $mode = stack ]
then
	while [ $# -gt 0 ]
	do	dec="`printf %d $1`"
		$NM -n $executable | grep ' [Tt] [^.]' | awk '
		  {  if($1 > '$dec') { printf "%s+0x%x\n", name, '$dec'-offset; exit }
		     name=$3; offset=$1
		  }'
		shift
	done

	exit 0
fi

# Invoked as datasizes?
if [ $mode = data ]
then
	$NM -n $executable |
		grep ' [bBdD] [^.]' | awk '{ if (lastpos) printf "%10ld kB  %s\n", ($1-lastpos)/1024, lastname; lastpos=$1; lastname=$3 }' | sort -n

	exit 0
fi

# Can't happen.
echo "Impossible invocation."

exit 1

