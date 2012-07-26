#!/usr/bin/perl
#
# Copyright (C) 2006, 2007, 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $Id: updateopenssl.pl,v 1.14 2010-12-22 04:05:41 marka Exp $

# updateopenssl.pl
# This script locates the latest version of OpenSSL in the grandparent
# directory and updates the build scripts to use that version.
#
# Path and directory
$path = "..\\..\\";

# List of files that need to be updated with the actual version of the
# openssl directory
@filelist = ("SetupLibs.bat",
	     "../lib/dns/win32/libdns.mak",
             "../lib/dns/win32/libdns.dsp",
	     "../bin/named/win32/named.mak",
	     "../bin/named/win32/named.dsp");

# Locate the openssl directory
$substr = getdirectory();
if ($substr eq 0) {
     print "No directory found\n";
}
else {
     print "Found $substr directory\n";
}
#Update the list of files
if ($substr ne 0) {
   $ind = 0;
   foreach $file (@filelist) {
        print "Updating file $file\n";
	updatefile($file, $substr);
	$ind++;
   }
}

# Function to find the
sub getdirectory {
    my(@namelist);
    my($file, $name);
    my($cnt);
    opendir(DIR,$path) || die "No Directory: $!";
    @namelist = grep (/^openssl-[0-9]+\.[0-9]+\.[0-9]+[a-z]{0,1}$/i, readdir(DIR));
    closedir(DIR);

    # Make sure we have something
    if (scalar(@namelist) == 0) {
        return (0);
    }
    # Now see if we have a directory or just a file.
    # Make sure we are case insensitive
    foreach $file (sort {uc($a) cmp uc($b)} @namelist) {
        if (-d $path.$file) {
           $name = $file;
        }
    }

    # If we have one use it otherwise report the error
    # Note that we are only interested in the last one
    # since the sort should have taken care of getting
    # the latest
    if (defined($name)) {
        return ($name);
    }
    else {
        return (0);
    }
}

# function to replace the openssl directory name with the latest one
sub updatefile {
        my($filename, $substr, $line);
        my(@Lines);

        $filename = $_[0];
        $substr   = $_[1];

        open (RFILE, $filename) || die "Can't open file $filename: $!";
        @Lines = <RFILE>;
        close (RFILE);

        # Replace the string
        foreach $line (@Lines) {
                $line =~ s/openssl-[0-9]+\.[0-9]+\.[0-9]+[a-z]{0,1}/$substr/gi;
        }
        #update the file
        open (RFILE, ">$filename") || die "Can't open file $filename: $!";
        foreach $line (@Lines) {
               print RFILE $line;
        }
        close(RFILE);
}

