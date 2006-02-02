#!/bin/sh 

set -e

# No trailing slashes in the directory, because we want to base the
# package filename on it.
dir=`echo "$1" | sed 's/\/*$//'`

if [ $# -ne 2 ]
then	echo "Usage: $0 sourcedir packagedir"
	exit 1
fi

if [ ! -d "$dir" ]
then	echo "Error: $dir isn't a directory."
	exit 1
fi

here=`pwd`
srcdir=$here/$dir
packagestart=$srcdir/now
findlist=$srcdir/findlist
tarfile=${dir}.tar
tar=$srcdir/$tarfile
tarbz=$tar.bz
pdir="$2"

mkdir $pdir 2>/dev/null || true
binsizes big
touch $packagestart
sleep 1
cd $dir

if [ ! -f build ]
then	echo "Error: No build script in $dir."
	exit 1
fi

sh -e build
cd /
echo " * Building package"
find / -cnewer $packagestart | grep -v "^$srcdir" | grep -v "^/dev" | grep -v "^/tmp" | grep -v "^/usr/tmp" | grep -v "^/usr/log" | grep -v "^/usr/adm" | grep -v "^/etc/utmp" | grep -v "^/usr/src" | pax -w -d | bzip2 >$tarbz
rm -f $packagestart $findlist $tarcmd
binsizes normal
mv $tarbz $here/$pdir
exit 0
