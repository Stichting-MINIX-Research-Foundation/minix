#!/bin/sh
. releasetools/netbsd2minix/common.sh

cd $N2M/whitelist
for $item in `ls`
do
	$N2M/applywhitelist.sh $item < $item
done

cd ..
./applywhitelist $MINIX < minix.txt
