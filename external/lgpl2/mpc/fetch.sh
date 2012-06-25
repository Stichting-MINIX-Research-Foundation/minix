#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`
url="http://www.multiprecision.org/mpc/download/mpc-0.9.tar.gz"

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f mpc-0.9.tar.gz ];
        then
                which curl >/dev/null
                if [ $? -eq 0 ]; then
                        curl -O $url
                else
                        # Default to wget
                        wget $url
                fi
        fi


	tar -xf mpc-0.9.tar.gz
	mv mpc-0.9 dist
fi

