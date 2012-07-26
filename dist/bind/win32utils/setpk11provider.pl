#!/usr/bin/perl
#
# Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: setpk11provider.pl,v 1.2 2009-10-12 16:41:13 each Exp $

# setpk11provider.pl
# This script sets the PKCS#11 provider name in the build scripts.
#
# for instance: setpk11provider.pl bp201w32HSM
#

if ($#ARGV != 0) {
	die "Usage: perl setpk11provider.pl <pkcs11_provider_dll_name>\n"
}

my $provider=$ARGV[0];

$provider =~ s|\.[dD][lL][lL]$||;

# List of files that need to be updated
@filelist = ("../bin/pkcs11/win32//pk11keygen.mak",
             "../bin/pkcs11/win32//pk11keygen.dsp",
	     "../bin/pkcs11/win32//pk11list.mak",
             "../bin/pkcs11/win32//pk11list.dsp",
	     "../bin/pkcs11/win32//pk11destroy.mak",
             "../bin/pkcs11/win32//pk11destroy.dsp");

# function to replace the provider define
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
                $line =~ s/unknown_provider/$substr/gi;
        }
        #update the file
        open (RFILE, ">$filename") || die "Can't open file $filename: $!";
        foreach $line (@Lines) {
               print RFILE $line;
        }
        close(RFILE);
}

# update config.h to define or undefine USE_PKCS11
sub updateconfig {
   my($havexml, $substr, $line);
   my(@Lines);

   $havexml = $_[0];

   open (RFILE, "../config.h") || die "Can't open config.h";
   @Lines = <RFILE>;
   close (RFILE);

   foreach $line (@Lines) {
      if ($havexml) {
         $line =~ s/^.*#undef USE_PKCS11.*$/define USE_PKCS11 1/;
      } else {
         $line =~ s/^#define USE_PKCS11 .*$/\/\* #undef USE_PKCS11 \*\//;
      }
   }

   open (RFILE, ">../config.h") || die "Can't open config.h";
   print "Updating file ../config.h\n";
   foreach $line (@Lines) {
      print RFILE $line;
   }
   close(RFILE);
}

#Update the list of files
if ($provider ne 0) {
   $ind = 0;
   print "Provider is $provider\n";
   foreach $file (@filelist) {
        print "Updating file $file\n";
	updatefile($file, $provider);
	$ind++;
   }
   updateconfig(1);
} else {
   updateconfig(0);
}

