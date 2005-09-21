#!/bin/sh

# This script gets and installs a package from the Website.
# It is called by getpack package1 ...
# A package must be in the form of pack.tar.bz2 and must
# include a build script that makes and installs it.
# The build script should succeed if installation works, else fail

# Examples:
#	easypack awk elle telnet	# fetch and install 3 packages
#	easypack -o awk elle telnet	# fetch and replace existing packs

SOURCE_DIR=/usr/src/commands		# where the source is deposited
OVERWRITE=0				# can an installed package be overwritten?
SOFTWARE_DIR="http://www.minix3.org/software"	# Tested and approved S/W
BETA_DIR="http://www.minix3.org/beta_software"	# Untested software


# Check for at least one parameter
case $# in
0)	echo Usage: $0 package ...
	exit ;;
esac

# Change to source directory
ORIG_DIR=`pwd`
rm -rf Log			# remove old debugging log
cd $SOURCE_DIR

# Check for write permission here
if test ! -w . 
   then echo You do not have write permission for $SOURCE_DIR
   exit 1
fi

# Check for -o flag; if found, set OVERWRITE
if test $1 = "-o"
   then OVERWRITE=1
        shift
fi

# Loop on the packages
for i
do # Check to see if it exists. Don't overwrite unless -o given
   echo " " ; echo Start fetching $i 
   echo " " >>$ORIG_DIR/Log
   echo ------------- Start fetching $i ------------------ >>$ORIG_DIR/Log
   if test -r $i
      then # Directory already exists. May it be overwritten?
	   if test $OVERWRITE = 0
              then echo $i already exists. Skipping this package
                   continue
	      else # Remove the directory
		   rm -rf $i
		   echo Existing directory $i removed
	   fi
    fi

   # Remove any junk from previous attempts
   rm -rf $i.tar.bz2 $i.tar

   # Get the package
   URL=$SOFTWARE_DIR/$i.tar.bz2
   URL1=$URL
   urlget $URL >$i.tar.bz2

   # See if we got the file or an error
   if grep "<HTML>" $i.tar.bz2 >/dev/null
      then # It is not in the directory of tested software. Try beta dir.
	   URL=$BETA_DIR/$i.tar.bz2
	   urlget $URL >$i.tar.bz2
	   if grep "<HTML>" $i.tar.bz2 >/dev/null
	      then echo Cannot get $i.
		   echo "   " Tried $URL1
		   echo "   " Tried $URL
		   echo "   " Skipping this package
		   rm -rf $i.tar.bz2
		   continue
	   fi
   fi

   # We got it. Unpack it.
   bunzip2 $i.tar.bz2 || smallbunzip2 $i.tar.bz2
   tar xf $i.tar
   if test ! -d $i
      then echo Unable to unpack $i
	   continue
   fi

   # It is now unpacked. Build it
   cd $i
   if sh build >>$ORIG_DIR/Log 2>&1
      then echo $i installed from $URL
      else echo $i failed to install
   fi

   # Clean up
   cd ..
#   rm -rf $i.tar*
done

