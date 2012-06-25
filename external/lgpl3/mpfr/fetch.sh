#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`
url="http://www.mpfr.org/mpfr-current/mpfr-3.1.0.tar.bz2"

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f mpfr-3.1.0.tar.bz2 ];
        then
                which curl >/dev/null
                if [ $? -eq 0 ]; then
                        curl -O $url
                else
                        # Default to wget
                        wget $url
                fi
        fi


	tar -xf mpfr-3.1.0.tar.bz2
	mv mpfr-3.1.0 dist
fi

