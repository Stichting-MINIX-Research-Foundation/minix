#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`
url="ftp://ftp.gmplib.org/pub/gmp-5.0.2/gmp-5.0.2.tar.bz2"

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f gmp-5.0.2.tar.bz2 ];
        then
                which curl >/dev/null
                if [ $? -eq 0 ]; then
                        curl -O $url
                else
                        # Default to wget
                        wget $url
                fi
        fi


	tar -xf gmp-5.0.2.tar.bz2
	mv gmp-5.0.2 dist
fi

