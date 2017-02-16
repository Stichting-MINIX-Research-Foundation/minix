#!/bin/bash
#
# dhclient-tz-exithook.sh
# Version 1.01 elear
#
# Copyright (c) 2007, Cisco Systems, Inc.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met: 
# 
#    - Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer. 
# 
#    - Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#
#    - Neither the name of Cisco Systems, Inc. nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#    
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
#    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
#    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# the following script is used to set the timezone based on the new
# dhcp timezone option defined currently in the IETF document
# draft-ietf-dhc-timezone-option-04.txt.

# this code is intended for use with ISC's dhclient.  it is to be called
# either as, or by, dhclient-exit-hooks
#
# As this is test code, in order for it to be called two changes
# must be made to /etc/dhclient.conf.  First, dhclient.conf must be
# aware of the tzName option.  The IANA has assigned tzName option
# code 101.  You may need to add this to your configuration file.
#
#  option tzName code 101 = text;
#
# Next, add tzName to the list of options in the "request" statement.
# For example:
# 
# request subnet-mask, broadcast-address, time-offset, routers,
#         domain-name, domain-name-servers, host-name, tzName;
#
#
# And of course make sure that your dhcp server is transmitting timezone
# information for option 101.  For IOS this can be done as follows:
# 
#    option 101 ascii "Europe/Berlin"
#

timefile=/etc/localtime
oldfile=$timefile.old
tmpfile=$timefile.$$

# function to clean up just in case we are interrupted or something
# bad happens.
restore_file () {

  if [ ! -f $timefile ]; then
     $DEBUG mv $tmpfile $timefile
  fi
  $DEBUG rm $tmpfile
  exit
}


#set DEBUG to "echo" to see what would happen.
if [ x$DEBUG = x ]; then
   DEBUG=
fi

# if something has already gone wrong we're not doing a thing.
if [ x$exit_status != x0 ]; then
   exit $exit_status
fi


# if we don't have a new timezone, then we have nothing to change, so
# goodbye.
if [ x$new_tzName = x ]; then
   exit 0
fi

# if the timezone doesn't exist, goodbye.
if [ ! -e $timefile ]; then
   exit 0
fi

# find zoneinfo. use the first one.
ftz=0
for a in /usr/share/zoneinfo /usr/lib/zoneinfo /var/share/zoneinfo /var/zoneinfo; do
  if [ -d $a -a $ftz = 0 ]; then
    zoneinfo=$a
    ftz=1
  fi
done

# no zoneinfo found.  goodbye.
if [ x$zoneinfo = x ]; then
   exit 0
fi

# timezone not found.  goodbye.
if [ ! -f $zoneinfo/$new_tzName ]; then
   exit 0
fi

# if we're here we can actually do something useful.
# first, link a copy of the existing timefile.

$DEBUG ln $timefile $tmpfile

if [ $? != 0 ]; then
  echo "unable to create temporary file"
  exit -1
fi

# in case of interrupt, cleanup.
trap restore_file SIGINT SIGSEGV SIGQUIT SIGTERM

# we destroy old backup files in this process.  if we cannot and the
# file exists then something went wrong.
if [ -e $oldfile ]; then
  $DEBUG rm $oldfile
  if [ $? != 0 ]; then
     echo "$0: failed to remove $oldfile"
     rm -f $tmpfile
     exit -1
  fi
fi

# sensitive part happens here:
#
$DEBUG mv $timefile $oldfile

  if [ $? != 0 ]; then
     echo "$0: failed to move old $timefile file out of the way"
     rm $tmpfile
     exit -1
  fi

$DEBUG ln $zoneinfo/$new_tzName $timefile

# we don't complain just yet- a hard link could fail because
# we're on two different file systems.  Go for a soft link.
#

if [ $? != 0 ]; then
  $DEBUG ln -s $zoneinfo/$new_tzName $timefile
fi

if [ $? != 0 ]; then       # failed to softlink.  now we're getting nervous.
  echo "$0: unable to establish new timezone.  Attempting to revert."
  $DEBUG ln $tmpfile $timefile
fi


if [ $? != 0 ]; then       # we're absolutely hosed
  echo "$0: unable to link or softlink timezone file, and unable to restore old file - giving up!"
  exit -1
fi

$DEBUG rm $tmpfile

exit $?
