#!/bin/sh

# This script can be used to install packages from the
# the installation CD-ROM.

RC=/usr/etc/rc.package
CDPATH=packages/$(uname -r)/$(uname -m)/All
PACKSUM=pkg_summary.bz2

# Run user rc script
if [ -f "$RC" ]
then
	. "$RC"
fi

# Mount CD
if [ -n "$cddrive" ]
then
	if [ -z $(mount | grep 'on /mnt ') ]
	then
		echo "Mounting $cddrive on /mnt."
		mount $cddrive /mnt
	fi
fi

# Find package summary
for i in / /mnt
do
	if [ -f $i/$CDPATH/$PACKSUM ]
	then
		(>&2 echo "Found package summary at $i/$CDPATH/$PACKSUM.")

		# Set package repo to CD and populate package db
		export PKG_REPOS=$i/$CDPATH/
		pkgin update

		# Run pkgin
		exec pkgin $@
	fi
done

echo "Can't find package summary. Please mount CD first at /mnt and make sure"
echo "that $CDPATH/$PACKSUM exists on the CD."
exit 1
