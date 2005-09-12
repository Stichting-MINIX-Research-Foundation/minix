#!/bin/sh

if [ $# -ne 1 ]
then	echo "Usage: $0 <url>"
	exit 1
fi

if [ -z "$HOME" ]
then	echo "Where is your \$HOME? "
	exit 1
fi

if [ ! -d "$HOME" ]
then	echo "Where is your \$HOME ($HOME) ? "
	exit 1
fi

tmpdir=$HOME/getpack$$
tmpfile=package
tmpfiletar=$tmpfile.tar
tmpfiletargz=$tmpfile.tar.gz

mkdir -m 700 $tmpdir || exit 1
cd $tmpdir || exit 1

urlget "$1" >$tmpfiletargz

gzip -d $tmpfiletargz || exit 1
tar xf $tmpfiletar || exit 1
make && make install && echo "Ok."
