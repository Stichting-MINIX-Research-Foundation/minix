#!/usr/bin/perl
#
# Copyright (C) 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: updatelibxml2.pl,v 1.5 2010-08-13 23:47:04 tbox Exp $

# updatelibxml2.pl
# This script locates the latest version of libxml2 in the grandparent
# directory and updates the build scripts to use that version.
# Copied from updateopenssl.pl.
#
# Path and directory
$path = "..\\..\\";

# List of files that need to be updated with the actual version of the
# libxml2 directory
@filelist = ("SetupLibs.bat",
             "../lib/dns/win32/libdns.mak",
             "../lib/dns/win32/libdns.dsp",
             "../bin/check/win32/checktool.dsp",
             "../bin/check/win32/namedcheckconf.dsp",
             "../bin/check/win32/namedcheckconf.mak",
             "../bin/check/win32/namedcheckzone.dsp",
             "../bin/check/win32/namedcheckzone.mak",
             "../bin/confgen/win32/confgentool.dsp",
             "../bin/confgen/win32/ddnsconfgen.dsp",
             "../bin/confgen/win32/ddnsconfgen.mak",
             "../bin/confgen/win32/rndcconfgen.dsp",
             "../bin/confgen/win32/rndcconfgen.mak",
             "../bin/dig/win32/dig.dsp",
             "../bin/dig/win32/dig.mak",
             "../bin/dig/win32/dighost.dsp",
             "../bin/dig/win32/host.dsp",
             "../bin/dig/win32/host.mak",
             "../bin/dig/win32/nslookup.dsp",
             "../bin/dig/win32/nslookup.mak",
             "../bin/dnssec/win32/dnssectool.dsp",
             "../bin/dnssec/win32/dsfromkey.dsp",
             "../bin/dnssec/win32/dsfromkey.mak",
             "../bin/dnssec/win32/keyfromlabel.dsp",
             "../bin/dnssec/win32/keyfromlabel.mak",
             "../bin/dnssec/win32/keygen.dsp",
             "../bin/dnssec/win32/keygen.mak",
             "../bin/dnssec/win32/revoke.dsp",
             "../bin/dnssec/win32/revoke.mak",
             "../bin/dnssec/win32/settime.dsp",
             "../bin/dnssec/win32/settime.mak",
             "../bin/dnssec/win32/signzone.dsp",
             "../bin/dnssec/win32/signzone.mak",
             "../bin/named/win32/named.dsp",
             "../bin/named/win32/named.mak",
             "../bin/nsupdate/win32/nsupdate.dsp",
             "../bin/nsupdate/win32/nsupdate.mak",
             "../bin/rndc/win32/rndc.dsp",
             "../bin/rndc/win32/rndc.mak",
             "../bin/tools/win32/journalprint.dsp",
             "../bin/tools/win32/journalprint.mak",
             "../lib/bind9/win32/libbind9.dsp",
             "../lib/bind9/win32/libbind9.mak",
             "../lib/dns/win32/libdns.dsp",
             "../lib/dns/win32/libdns.mak",
             "../lib/isc/win32/libisc.dsp",
             "../lib/isc/win32/libisc.mak",
	     "../lib/isc/win32/libisc.def",
             "../lib/isccc/win32/libisccc.dsp",
             "../lib/isccc/win32/libisccc.mak",
             "../lib/isccfg/win32/libisccfg.dsp",
             "../lib/isccfg/win32/libisccfg.mak");

# Locate the libxml2 directory
$substr = getdirectory();
if ($substr eq 0) {
     print "No directory found\n";
}
else {
     print "Found $substr directory\n";
}

if ($substr ne 0) {
   #Update the list of files
   $ind = 0;
   updateconfig(1);
   foreach $file (@filelist) {
      print "Updating file $file\n";
      updatefile($file, $substr, 1);
      $ind++;
   }
}
else {
   #Update the configuration to reflect libxml2 being absent
   $ind = 0;
   updateconfig(0);
   foreach $file (@filelist) {
      print "Updating file $file\n";
      updatefile($file, $substr, 0);
      $ind++;
   }
}

# Function to find the libxml2 directory
sub getdirectory {
    my(@namelist);
    my($file, $name);
    my($cnt);
    opendir(DIR,$path) || return (0);
    @namelist = grep (/^libxml2-[0-9]+\.[0-9]+\.[0-9]+[a-z]*$/i, readdir(DIR));
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

# function to replace the libxml2 directory name with the latest one
sub updatefile {
        my($filename, $substr, $line);
        my(@Lines);

        $filename = $_[0];
        $substr   = $_[1];
        $havexml  = $_[2];

        open (RFILE, $filename) || die "Can't open file $filename: $!";
        @Lines = <RFILE>;
        close (RFILE);

        # Replace the string
        foreach $line (@Lines) {
           if ($havexml) {
              $line =~ s/libxml2-[0-9]+\.[0-9]+\.[0-9]+[a-z]*/$substr/gi;
	      if ($filename =~ /\.mak$/) {
		 $line =~ s/^# (LIBXML=.*\/libxml2\.lib.*)$/\1/;
	      } elsif ($filename =~ /\.dsp$/ ) {
                 $line =~ s/^!MESSAGE (LIBXML=.*\/libxml2\.lib.*)$/\1/;
                 $line =~ s/^!MESSAGE (# ADD LINK32 .*\/libxml2\.lib.*)$/\1/;
	      }
	      $line =~ s/^; (isc_socketmgr_renderxml)$/\1/;
	      $line =~ s/^; (isc_mem_renderxml)$/\1/;
	      $line =~ s/^; (isc_taskmgr_renderxml)$/\1/;
           } else {
	      if ($filename =~ /\.mak$/) {
		 $line =~ s/^(LIBXML=.*\/libxml2.lib.*)$/# \1/i;
	      } elsif ($filename =~ /\.dsp$/ ) {
                 $line =~ s/^(# ADD LINK32 .*\/libxml2.lib.*)$/!MESSAGE \1/i;
                 $line =~ s/^(LIBXML=.*\/libxml2.lib.*)$/!MESSAGE \1/i;
	      }
	      $line =~ s/^(isc_socketmgr_renderxml)$/; \1/;
	      $line =~ s/^(isc_mem_renderxml)$/; \1/;
	      $line =~ s/^(isc_taskmgr_renderxml)$/; \1/;
           }
        }

        #update the file
        open (RFILE, ">$filename") || die "Can't open file $filename: $!";
        foreach $line (@Lines) {
           print RFILE $line;
        }
        close(RFILE);
}

# update config.h to define or undefine HAVE_LIBXML2
sub updateconfig {
   my($havexml, $substr, $line);
   my(@Lines);

   $havexml = $_[0];

   open (RFILE, "../config.h") || die "Can't open config.h";
   @Lines = <RFILE>;
   close (RFILE);

   foreach $line (@Lines) {
      if ($havexml) {
         $line =~ s/^.*#undef HAVE_LIBXML2.*$/define HAVE_LIBXML2 1/;
      } else {
         $line =~ s/^#define HAVE_LIBXML2 .*$/\/\* #undef HAVE_LIBXML2 \*\//;
      }
   }

   open (RFILE, ">../config.h") || die "Can't open config.h";
   print "Updating file ../config.h\n";
   foreach $line (@Lines) {
      print RFILE $line;
   }
   close(RFILE);
}
