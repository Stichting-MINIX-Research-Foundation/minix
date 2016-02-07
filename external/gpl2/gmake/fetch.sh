#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

FETCH=ftp
which curl >/dev/null
if [ $? -eq 0 ]; then
        FETCH="curl -O -f"
fi

# Configure fetch method - GMAKE
URL="http://www.minix3.org/pkgsrc/distfiles/minix/3.4.0/make-3.81.tar.bz2"
BACKUP_URL="ftp://ftp.gnu.org/gnu/make/make-3.81.tar.bz2"

# Fetch sources if not available
if [ ! -d dist ];
then
        if [ ! -f make-3.81.tar.bz2 ]; then
                $FETCH $URL
                if [ $? -ne 0 ]; then
                        $FETCH $BACKUP_URL
                fi
        fi

        tar -xjf make-3.81.tar.bz2 && \
        mv make-3.81 dist && \
        cd dist && \
        cat ../patches/* | patch -p 1 || true
fi
