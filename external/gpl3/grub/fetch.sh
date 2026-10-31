#!/bin/sh

# Make sure we're in our directory (i.e., where this shell script is)
echo $0
cd `dirname $0`

# Configure fetch method
URL="http://ftp.gnu.org/gnu/grub/grub-2.06.tar.gz"
FETCH=ftp
which curl >/dev/null
if [ $? -eq 0 ]; then
	FETCH="curl -O -f"
fi

# Fetch sources if not available
if [ ! -d dist ];
then
	if [ ! -f grub-2.06.tar.gz ]; then
		$FETCH $URL
		if [ $? -ne 0 ]; then
			$FETCH $BACKUP_URL
		fi
	fi

	tar -oxzf grub-2.06.tar.gz && \
	mv grub-2.06 dist && \
	cd dist && \
	sh ../build.sh
fi

