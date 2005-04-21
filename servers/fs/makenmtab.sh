#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin

echo "struct fs_sym_entry { unsigned long symoffset; char *symname; } fs_sym_entries[] = {" >$1
nm -n fs | grep -v "^fs:$" | grep ' [tT] ' | awk '{ print "{ 0x" $1 ", \"" $3 "\" }, " }' >>$1
echo "};" >>$1
