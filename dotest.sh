#!/bin/sh
export PATH=../obj.evbearm-el/tooldir.Linux-3.5.0-26-generic-x86_64/bin/:$PATH

#
# Convert the METALOG to something else
# -N etc points to the master.passwd and groups file
cat ../obj.evbearm-el/destdir.evbearm-el/METALOG.sanitised | nbmtree -N etc -C > input
#nbmake-evbearm-el -C tools/mkfs.mfs all install
#nbmkfs.mfs image ../obj.evbearm-el/destdir.evbearm-el/METALOG
