#!/bin/sh

# This script can be used to install packages from the
# the installation CD-ROM.

RC=/usr/etc/rc.package
CDMP=/mnt
CDPACK=${CDMP}/install/packages
PACKSUM=pkg_summary.bz2
cdpackages=""
cdmounted=""

if [ -f "$RC" ]
then   . "$RC"
fi

# Is there a usable CD to install packages from?
if [ -n "$cddrive" ]
then   pack=${cddrive}p2
       umount $pack >/dev/null 2>&1 || true
       echo "Checking for CD in $pack."
       if mount -r $pack $CDMP 2>/dev/null
       then    fn="$CDPACK/$PACKSUM"
               echo "Found."
               cdmounted=1
               cdpackages=$fn
               if [ ! -f $cdpackages ]
               then    cdpackages=""
                       echo "No package summary found on CD in $fn."
		       exit 1
               fi
       else    echo "Not found."
               exit 1
       fi
else   echo "Don't know where the install CD is. You can set it in $RC."
       exit 1
fi

# Set package repo to CD and populate package db
export PKG_REPOS=${CDPACK}
pkgin update

# Run pkgin
pkgin $@
