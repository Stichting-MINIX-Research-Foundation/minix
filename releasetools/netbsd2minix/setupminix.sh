#!/bin/sh
cd ../..
mv minix releasetools/netbsd2minix/netbsd

rm releasetools/netbsd2minix/netbsd/build.sh
mv build.sh releasetools/netbsd2minix/netbsd

rm -r
