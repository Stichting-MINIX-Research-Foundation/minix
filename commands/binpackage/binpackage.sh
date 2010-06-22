#!/bin/sh 

PATH=/bin:/sbin:/usr/bin:/usr/sbin
PI=.postinstall
INFO=.minixpackage

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
tarbz=$tar.bz2

mkdir $pdir 2>/dev/null || true
rc=$dir/.binpackage
if [ -f $rc ]
then	 . $rc
fi

prunedirs="$srcdir dev tmp usr/bigports usr/src usr/tmp usr/log usr/adm usr/run home etc/utmp var/run var/log /var/spool"

for d in $prunedirs
do	pruneexpr="$pruneexpr $n -path /$d -prune"
	n="-o "
done

touch $packagestart
sleep 1
cd $dir

if [ ! -f build -a ! -f build.minix ]
then	echo "Error: No build or build.minix script in $dir."
	exit 1
fi

if [ -f build.minix ]
then	sh -e build.minix
else	sh -e build
fi

echo " * Building package"
echo "Minix package $dir built `date`." >$INFO
(
	echo $INFO
	if [ -f $PI ]
	then	echo $PI
	fi
	find / \( $pruneexpr \) -o -cnewer $packagestart -print | fgrep -v /.svn
) | pax -w -d | bzip2 >$tarbz
rm -f $packagestart $findlist $tarcmd
mv $tarbz $pdir
exit 0
