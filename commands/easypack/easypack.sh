#!/bin/sh

# This script gets and installs a package from the Website.
# It is called by easypack package1 ...
# A package must be in the form of pack.tar.bz2 and must
# include a build script that makes and installs it.
# The build script should succeed if installation works, else fail

# Examples:
#	easypack awk elle telnet	# fetch and install 3 packages
#	easypack -o awk elle telnet	# fetch and replace existing packs

SOURCE_DIR=/usr/local/src		# where the source is deposited
OVERWRITE=0				# can an installed package be overwritten?
SOFTWARE_DIR="http://www.minix3.org/software"


# Check for at least one parameter
case $# in
0)	echo Usage: $0 package ...
	exit ;;
esac

# Change to source directory
ORIG_DIR=`pwd`
rm -f Log			# remove old debugging log
mkdir $SOURCE_DIR || true
cd $SOURCE_DIR || exit

if [ "`id -u`" -ne 0 ]
then
	# Check for write permission here
	if test ! -w . 
	   then echo You do not have write permission for $SOURCE_DIR
	   exit 1
	fi
fi

# Check for -o flag; if found, set OVERWRITE
if test $1 = "-o"
   then OVERWRITE=1
        shift
fi

# Loop on the packages
for i
do # Check to see if it exists. Don't overwrite unless -o given
   echo " " ; echo Start fetching package $i 
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
   rm -f $i.tar.bz2 $i.tar

   # Get the package
   URL=$SOFTWARE_DIR/$i.tar.bz2
   TARBZ=$i.tar.bz2
   if urlget $URL >$TARBZ 2>/dev/null
   then :
   else 
	echo Cannot get $i.
	echo "   " Tried $URL
	echo "   " Skipping this package
	rm -f $TARBZ
	continue
   fi

   # We got it. Unpack it.
   echo Package $i fetched
   bunzip2 $TARBZ || smallbunzip2 $TARBZ
   pax -r <$i.tar
   if test ! -d $i
      then echo Unable to unpack $i
	   continue
      else echo Package $i unpacked
   fi

   # It is now unpacked. Build it
   cd $i
   binsizes big
   if [ -f build.minix ]
   then	sh build.minix >>$ORIG_DIR/Log 2>&1
	r=$?
   else	sh build >>$ORIG_DIR/Log 2>&1
	r=$?
   fi
   if [ $r -eq 0 ] 
      then echo Package $i installed
      else echo Package $i failed to install, see Log
   fi
   if [ -f .postinstall ]
   then	echo Running postinstall script.
	sh -e .postinstall
   fi
   binsizes normal

   # Clean up
   cd ..
   rm -f $i.tar $TARBZ # Remove whatever is still lying around
done

