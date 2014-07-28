#!/bin/sh

# Look at /usr/pkg/bin first in case there is an old nm in /usr/bin
PATH=/usr/pkg/bin:$PATH:/usr/gnu/bin

# Does procfs give us some extra 'symbols'?
IPCVECS=/proc/ipcvecs
if [ -f $IPCVECS ]
then	EXTRANM="cat $IPCVECS"
fi

# Check usage
if [ $# -lt 1 ]
then	echo "Usage: unstack <executable> [0x... [0x... ] ]"
	exit 1
fi

# Check invocation mode
case "`basename $0`" in
	datasizes)
		echo "datasizes is obsolete; please use nm --size-sort instead."
		exit 1
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

if ! which gawk >/dev/null 2>&1
then	echo "Please install gawk."
	exit 1
fi

# Invoke binutils nm or ack nm?
if file $executable | grep ELF >/dev/null 2>&1
then	NM="nm"
else	NM="acknm"
fi

SYMLIST=/tmp/unstack.$$

# store sorted, filtered nm output once
(
$NM $executable | sed 's/^/0x/'
$EXTRANM | sed 's/^/0x/'

# Add shared libraries
ldd -f"%p %x\n" $executable 2>/dev/null | while read lib addr
do	echo >&2 "Adding $lib at $addr"
	nm -n $lib | sed 's/^/0x/' | grep ' [Tt] ' | \
gawk -v liboffset=$addr -v libname=$lib --non-decimal-data '{ printf "0x%lx %s %s[%s]\n", $1+liboffset, $2, $3, libname }'
done
) | sort -x | grep ' [Tt] [^.]' >$SYMLIST

while [ $# -gt 0 ]
do	 gawk <$SYMLIST --non-decimal-data -v symoffset=$1 '
	  {  if($1 > symoffset) { printf "%s+0x%x\n", name, symoffset-prevoffset; exit }
	     name=$3; prevoffset=$1;
	  }'
	shift
done

rm -f $SYMLIST

exit 1

