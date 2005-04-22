#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
cd /usr || exit 1
tar cf src.tar src || exit 1
cd /usr/src || exit 1
make world install || exit 1
cd tools || exit 1
make hdboot
cd /usr || exit 1
rm -rf /usr/src || exit 1
exit 0

