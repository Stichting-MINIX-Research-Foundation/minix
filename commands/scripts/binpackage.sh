#!/bin/sh 

PI=.postinstall

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

case "$dir" in
/*) srcdir="$dir" ;;
*) srcdir="$here/$dir" ;;
esac

case $2 in
/*) pdir="$2" ;;
*) pdir="$here/$2" ;;
esac

packagestart=$srcdir/now
findlist=$srcdir/findlist
tar=$srcdir/"`basename ${dir}`".tar
tarbz=$tar.bz

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
echo " * Building package"
( if [ -f $PI ]; then echo $PI; fi; find / -cnewer $packagestart | egrep -v "^($srcdir|/(dev|tmp)|/usr/(tmp|log|adm|run|src)|/etc/utmp|/var/run)" ) | pax -w -d | bzip2 >$tarbz
rm -f $packagestart $findlist $tarcmd
binsizes normal
mv $tarbz $pdir
exit 0
