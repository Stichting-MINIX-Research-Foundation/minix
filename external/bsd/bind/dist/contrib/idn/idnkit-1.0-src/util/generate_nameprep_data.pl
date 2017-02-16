#! /usr/local/bin/perl -w
# Id: generate_nameprep_data.pl,v 1.1 2003/06/04 00:27:54 marka Exp 
#
# Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
#  
# By using this file, you agree to the terms and conditions set forth bellow.
# 
# 			LICENSE TERMS AND CONDITIONS 
# 
# The following License Terms and Conditions apply, unless a different
# license is obtained from Japan Network Information Center ("JPNIC"),
# a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
# Chiyoda-ku, Tokyo 101-0047, Japan.
# 
# 1. Use, Modification and Redistribution (including distribution of any
#    modified or derived work) in source and/or binary forms is permitted
#    under this License Terms and Conditions.
# 
# 2. Redistribution of source code must retain the copyright notices as they
#    appear in each source code file, this License Terms and Conditions.
# 
# 3. Redistribution in binary form must reproduce the Copyright Notice,
#    this License Terms and Conditions, in the documentation and/or other
#    materials provided with the distribution.  For the purposes of binary
#    distribution the "Copyright Notice" refers to the following language:
#    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
# 
# 4. The name of JPNIC may not be used to endorse or promote products
#    derived from this Software without specific prior written approval of
#    JPNIC.
# 
# 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
#    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
#    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
#

use v5.6.0;		# for pack('U')
use bytes;

use lib qw(.);

use SparseMap;
use Getopt::Long;

(my $myid = 'Id: generate_nameprep_data.pl,v 1.1 2003/06/04 00:27:54 marka Exp $') =~ s/\$([^\$]+)\$/\$-$1-\/;

my @map_bits = (9, 7, 5);
my @proh_bits = (7, 7, 7);
my @unas_bits = (7, 7, 7);
my @bidi_bits = (9, 7, 5);

my @bidi_types = ('OTHERS', 'R_AL', 'L');

my $dir = '.';
my @versions = ();

GetOptions('dir=s', \$dir) or die usage();
@versions = @ARGV;

print_header();

bits_definition("MAP", @map_bits);
bits_definition("PROH", @proh_bits);
bits_definition("UNAS", @unas_bits);
bits_definition("BIDI", @bidi_bits);

generate_data($_) foreach @ARGV;

sub usage {
    die "Usage: $0 [-dir dir] version..\n";
}

sub generate_data {
    my $version = shift;
    generate_mapdata($version, "$dir/nameprep.$version.map");
    generate_prohibiteddata($version, "$dir/nameprep.$version.prohibited");
    generate_unassigneddata($version, "$dir/nameprep.$version.unassigned");
    generate_bididata($version, "$dir/nameprep.$version.bidi");
}

#
# Generate mapping data.
#
sub generate_mapdata {
    my $version = shift;
    my $file = shift;

    my $map = SparseMap::Int->new(BITS => [@map_bits],
				  MAX => 0x110000,
				  MAPALL => 1,
				  DEFAULT => 0);
    open FILE, $file or die "cannot open $file: $!\n";

    my $mapbuf = "\0";	# dummy
    my %maphash = ();
    while (<FILE>) {
	if ($. == 1 and /^%\s*SAME-AS\s+(\S+)/) {
	    my $same_as = $1;
	    if (grep {$_ eq $same_as} @versions > 0) {
		generate_map_ref($version, $same_as);
		close FILE;
		return;
	    }
	    next;
	}
	next if /^\#/;
	next if /^\s*$/;
	register_map($map, \$mapbuf, \%maphash, $_);
    }
    close FILE;
    generate_map($version, $map, \$mapbuf);
}

#
# Generate prohibited character data.
#
sub generate_prohibiteddata {
    my $version = shift;
    my $file = shift;

    my $proh = SparseMap::Bit->new(BITS => [@proh_bits],
				   MAX => 0x110000);
    open FILE, $file or die "cannot open $file: $!\n";
    while (<FILE>) {
	if ($. == 1 and /^%\s*SAME-AS\s+(\S+)/) {
	    my $same_as = $1;
	    if (grep {$_ eq $same_as} @versions > 0) {
		generate_prohibited_ref($version, $same_as);
		close FILE;
		return;
	    }
	    next;
	}
	next if /^\#/;
	next if /^\s*$/;
	register_prohibited($proh, $_);
    }
    close FILE;
    generate_prohibited($version, $proh);
}

#
# Generate unassigned codepoint data.
#
sub generate_unassigneddata {
    my $version = shift;
    my $file = shift;

    my $unas = SparseMap::Bit->new(BITS => [@unas_bits],
				   MAX => 0x110000);
    open FILE, $file or die "cannot open $file: $!\n";
    while (<FILE>) {
	if ($. == 1 and /^%\s*SAME-AS\s+(\S+)/) {
	    my $same_as = $1;
	    if (grep {$_ eq $same_as} @versions > 0) {
		generate_unassigned_ref($version, $same_as);
		close FILE;
		return;
	    }
	    next;
	}
	next if /^\#/;
	next if /^\s*$/;
	register_unassigned($unas, $_);
    }
    close FILE;
    generate_unassigned($version, $unas);
}

#
# Generate data of bidi "R" or "AL" characters.
#
sub generate_bididata {
    my $version = shift;
    my $file = shift;

    my $bidi = SparseMap::Int->new(BITS => [@bidi_bits],
				   MAX => 0x110000);
    open FILE, $file or die "cannot open $file: $!\n";

    my $type = 0;
    while (<FILE>) {
	if ($. == 1 and /^%\s*SAME-AS\s+(\S+)/) {
	    my $same_as = $1;
	    if (grep {$_ eq $same_as} @versions > 0) {
		generate_unassigned_ref($version, $same_as);
		close FILE;
		return;
	    }
	    next;
	}
	if (/^%\s*BIDI_TYPE\s+(\S+)$/) {
	    my $i = 0;
	    for ($i = 0; $i < @bidi_types; $i++) {
		if ($1 eq $bidi_types[$i]) {
		    $type = $i;
		    last;
		}
	    }
	    die "unrecognized line: $_" if ($i >= @bidi_types);
	    next;
	}
	next if /^\#/;
	next if /^\s*$/;
	register_bidi($bidi, $type, $_);
    }
    close FILE;

    generate_bidi($version, $bidi);
}

sub print_header {
    print <<"END";
/* \Id\ */
/* $myid */
/*
 * Do not edit this file!
 * This file is generated from NAMEPREP specification.
 */

END
}

sub bits_definition {
    my $name = shift;
    my @bits = @_;
    my $i = 0;

    foreach my $n (@bits) {
	print "#define ${name}_BITS_$i\t$n\n";
	$i++;
    }
    print "\n";
}

sub register_map {
    my ($map, $bufref, $hashref, $line) = @_;

    my ($from, $to) = split /;/, $line;
    my @fcode = map {hex($_)} split ' ', $from;
    my @tcode = map {hex($_)} split ' ', $to;

    my $ucs4 = pack('V*', @tcode);
    $ucs4 =~ s/\000+$//;

    my $offset;
    if (exists $hashref->{$ucs4}) {
	$offset = $hashref->{$ucs4};
    } else {
	$offset = length $$bufref;
	$$bufref .= pack('C', length($ucs4)) . $ucs4;
	$hashref->{$ucs4} = $offset;
    }

    die "unrecognized line: $line" if @fcode != 1;
    $map->add($fcode[0], $offset);
}

sub generate_map {
    my ($version, $map, $bufref) = @_;

    $map->fix();

    print $map->cprog(NAME => "nameprep_${version}_map");
    print "\nstatic const unsigned char nameprep_${version}_map_data[] = \{\n";
    print_uchararray($$bufref);
    print "};\n\n";
}

sub generate_map_ref {
    my ($version, $refversion) = @_;
    print <<"END";
#define nameprep_${version}_map_imap	nameprep_${refversion}_map_imap
#define nameprep_${version}_map_table	nameprep_${refversion}_map_table
#define nameprep_${version}_map_data	nameprep_${refversion}_map_data

END
}

sub print_uchararray {
    my @chars = unpack 'C*', $_[0];
    my $i = 0;
    foreach my $v (@chars) {
	if ($i % 12 == 0) {
	    print "\n" if $i != 0;
	    print "\t";
	}
	printf "%3d, ", $v;
	$i++;
    }
    print "\n";
}

sub register_prohibited {
    my $proh = shift;
    register_bitmap($proh, @_);
}

sub register_unassigned {
    my $unas = shift;
    register_bitmap($unas, @_);
}

sub register_bidi {
    my $bidi = shift;
    my $type = shift;
    register_intmap($bidi, $type, @_);
}

sub generate_prohibited {
    my ($version, $proh) = @_;
    generate_bitmap($proh, "nameprep_${version}_prohibited");
    print "\n";
}

sub generate_prohibited_ref {
    my ($version, $refversion) = @_;
    print <<"END";
#define nameprep_${version}_prohibited_imap	nameprep_${refversion}_prohibited_imap
#define nameprep_${version}_prohibited_bitmap	nameprep_${refversion}_prohibited_bitmap

END
}

sub generate_unassigned {
    my ($version, $unas) = @_;
    generate_bitmap($unas, "nameprep_${version}_unassigned");
    print "\n";
}

sub generate_unassigned_ref {
    my ($version, $refversion) = @_;
    print <<"END";
#define nameprep_${version}_unassigned_imap	nameprep_${refversion}_unassigned_imap
#define nameprep_${version}_unassigned_bitmap	nameprep_${refversion}_unassigned_bitmap

END
}

sub generate_bidi {
    my ($version, $bidi) = @_;

    $bidi->fix();

    print $bidi->cprog(NAME => "nameprep_${version}_bidi");
    print "\n";
    print "static const unsigned char nameprep_${version}_bidi_data[] = \{\n";

    foreach my $type (@bidi_types) {
	printf "\tidn_biditype_%s, \n", lc($type);
    }
    print "};\n\n";
}

sub generate_bidi_ref {
    my ($version, $refversion) = @_;
    print <<"END";
#define nameprep_${version}_bidi_imap	nameprep_${refversion}_bidi_imap
#define nameprep_${version}_bidi_table	nameprep_${refversion}_bidi_table

END
}

sub register_bitmap {
    my $map = shift;
    my $line = shift;

    /^([0-9A-Fa-f]+)(?:-([0-9A-Fa-f]+))?/ or die "unrecognized line: $line";
    my $start = hex($1);
    my $end = defined($2) ? hex($2) : undef;
    if (defined $end) {
	$map->add($start .. $end);
    } else {
	$map->add($start);
    }
}

sub register_intmap {
    my $map = shift;
    my $value = shift;
    my $line = shift;

    /^([0-9A-Fa-f]+)(?:-([0-9A-Fa-f]+))?/ or die "unrecognized line: $line";
    my $start = hex($1);
    my $end = defined($2) ? hex($2) : $start;
    for (my $i = $start; $i <= $end; $i++) {
	$map->add($i, $value);
    }
}

sub generate_bitmap {
    my $map = shift;
    my $name = shift;
    $map->fix();
    #$map->stat();
    print $map->cprog(NAME => $name);
}
