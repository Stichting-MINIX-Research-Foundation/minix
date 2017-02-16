#!/usr/bin/perl
#
# Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: mkzonefile.pl,v 1.2 2011/09/02 21:15:35 each Exp 
use strict;

die "Usage: makenames.pl zonename num_records" if (@ARGV != 2);
my $zname = @ARGV[0];
my $nrecords = @ARGV[1];

my @chars = split("", "abcdefghijklmnopqrstuvwxyz");

print"\$TTL 300	; 5 minutes
\$ORIGIN $zname.
@			IN SOA	mname1. . (
				2011080201 ; serial
				20         ; refresh (20 seconds)
				20         ; retry (20 seconds)
				1814400    ; expire (3 weeks)
				600        ; minimum (1 hour)
				)
			NS	ns
ns			A	10.53.0.3\n";

srand; 
for (my $i = 0; $i < $nrecords; $i++) {
        my $name = "";
        for (my $j = 0; $j < 8; $j++) {
                my $r = rand 25;
                $name .= $chars[$r];
        }
        print "$name" . "\tIN\tA\t";
        my $x = int rand 254;
        my $y = int rand 254;
        my $z = int rand 254;
        print "10.$x.$y.$z\n";
}

