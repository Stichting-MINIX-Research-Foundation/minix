#!/bin/sh
: List all system directories containing the argument
: Author: Terrence W. Holm
if test $# -ne 1; then
  echo "Usage:  whereis  name"
  exit 1
fi

path="/bin /lib /etc\
      /usr/bin /usr/lib\
      /usr/include /usr/include/sys"

for dir in $path; do
  for file in $dir/$1 $dir/$1.*; do
    if test -f $file; then
      echo $file 
    elif test -d $file; then
      echo $file/
      fi
    done
  done

exit 0
