#!/bin/sh

DIST=/usr/dist
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin

cd /usr/src || exit 1
mkdir -p $DIST
#echo "Making src.tar"
#tar cf /dist/src.tar /usr/src || exit 1
make world install || exit 1

cd tools || exit 1
make hdboot

#rm -rf /usr/src
#echo "Making bin.tar"
#tar cf /dist/bin.tar /

exit 0

