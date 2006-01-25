#!/bin/sh
cat $1 | while read line
do	 echo $line | awk 'NF==4 { print "mkdir -p "$4" || exit 1; chmod "$1" "$4" || exit 1; chown "$2" "$4" || exit 1; chgrp "$3" "$4" || exit 1" } NF==3 { print "rm "$1" ; ln -s "$3" "$1" || exit 1" } ' | sh || exit 1
done
