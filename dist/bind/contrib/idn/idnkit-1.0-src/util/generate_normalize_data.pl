#! /usr/local/bin/perl -w
# $Id: generate_normalize_data.pl,v 1.1.1.1 2003-06-04 00:27:55 marka Exp $
#
# Copyright (c) 2000,2001 Japan Network Information Center.
# All rights reserved.
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

# 
# Generate lib/unicodedata.c from UnicodeData.txt,
# CompositionExclusions-1.txt, SpecialCasing.txt and CaseFolding.txt,
# all of them available from ftp://ftp.unicode.org/Public/UNIDATA/.
#

use strict;
use lib qw(.);

use Getopt::Long;
use UCD;
use SparseMap;

use constant UCS_MAX => 0x110000;
use constant END_BIT => 0x80000000;

my $DECOMP_COMPAT_BIT = 0x8000;

my $CASEMAP_FINAL_BIT = 0x1;
my $CASEMAP_NONFINAL_BIT = 0x2;
my $CASEMAP_LAST_BIT = 0x10;

my $LETTER_BIT = 1;
my $NSPMARK_BIT = 2;

(my $myid = '$Id: generate_normalize_data.pl,v 1.1.1.1 2003-06-04 00:27:55 marka Exp $') =~ s/\$([^\$]+)\$/\$-$1-\$/;

my @default_bits = (9, 7, 5);
#my @default_bits = (7, 7, 7);
my @canon_class_bits = @default_bits;
my @decomp_bits = @default_bits;
my @comp_bits = @default_bits;
my @folding_bits = @default_bits;
my @casemap_bits = @default_bits;
my @casemap_ctx_bits = @default_bits;

my $prefix = '';
my $dir = '.';
my $unicodedatafile = 'UnicodeData.txt';
my $exclusionfile = 'CompositionExclusions.txt';
my $specialcasefile = 'SpecialCasing.txt';
my $casefoldingfile = 'CaseFolding.txt';
my $verbose;

GetOptions('dir|d=s' => \$dir,
	   'unicodedata|u=s' => \$unicodedatafile,
	   'exclude|e=s' => \$exclusionfile,	
	   'specialcase|s=s' => \$specialcasefile,
	   'casefold|c=s' => \$casefoldingfile,
	   'prefix|p=s' => \$prefix,
	   'verbose|v' => \$verbose,
) or usage();

foreach my $r (\$unicodedatafile, \$exclusionfile,
	       \$specialcasefile, \$casefoldingfile) {
    $$r = "$dir/$$r" unless $$r =~ m|^/|;
}

my %exclusions;
my %lower_special;
my %upper_special;

my @decomp_data;
my @comp_data;
my @toupper_data;
my @tolower_data;
my @folding_data;

#
# Create Mapping/Bitmap objects.
#

# canonical class
my $canon_class = SparseMap::Int->new(BITS => [@canon_class_bits],
				     MAX => UCS_MAX,
				     MAPALL => 1,
				     DEFAULT => 0);

# canonical/compatibility decomposition
my $decomp = SparseMap::Int->new(BITS => [@decomp_bits],
				 MAX => UCS_MAX,
				 MAPALL => 1,
				 DEFAULT => 0);

# canonical composition
my $comp = SparseMap::Int->new(BITS => [@comp_bits],
			       MAX => UCS_MAX,
			       MAPALL => 1,
			       DEFAULT => 0);

# uppercase/lowercase
my $upper = SparseMap::Int->new(BITS => [@casemap_bits],
			        MAX => UCS_MAX,
			        MAPALL => 1,
			        DEFAULT => 0);
my $lower = SparseMap::Int->new(BITS => [@casemap_bits],
			        MAX => UCS_MAX,
			        MAPALL => 1,
			        DEFAULT => 0);

# final/nonfinal context
my $casemap_ctx = SparseMap::Int->new(BITS => [@casemap_ctx_bits],
				      MAX => UCS_MAX,
				      MAPALL => 1,
				      DEFAULT => 0);

# casefolding
my $folding = SparseMap::Int->new(BITS => [@folding_bits],
				  MAX => UCS_MAX,
				  MAPALL => 1,
				  DEFAULT => 0);

#
# Read datafiles.
#

read_exclusion_file();
read_specialcasing_file();
read_unicodedata_file();
read_casefolding_file();

print_header();
print_canon_class();
print_composition();
print_decomposition();
print_casemap();
print_casemap_context();
print_casefolding();

exit;

sub usage {
    print STDERR <<"END";
Usage: $0 [options..]
  options:
    -d DIR  directory where Unicode Character Data files resides [./]
    -u FILE name of the UnicodeData file [UnicodeData.txt]
    -e FILE name of the CompositionExclusion file [CompositionExclusions-1.txt]
    -s FILE name of the SpecialCasing file [SpecialCasing.txt]
    -c FILE name of the CaseFolding file [CaseFolding.txt]
END
    exit 1;
}

#
# read_exclusion_file -- read CompositionExclusions-1.txt.
#
sub read_exclusion_file {
    open EXCLUDE, $exclusionfile   or die "cannot open $exclusionfile: $!\n";
    while ($_ = UCD::CompositionExclusions::getline(\*EXCLUDE)) {
	my %data = UCD::CompositionExclusions::parseline($_);
	$exclusions{$data{CODE}} = 1;
    }
    close EXCLUDE;
}

#
# read_specialcasing_file -- read SpecialCasing.txt
#
sub read_specialcasing_file {
    open SPCASE, $specialcasefile or die "cannot open $specialcasefile: $!\n";
    while ($_ = UCD::SpecialCasing::getline(\*SPCASE)) {
	my %data = UCD::SpecialCasing::parseline($_);
	my $code = $data{CODE};
	my $lower = $data{LOWER};
	my $upper = $data{UPPER};
	my $cond = $data{CONDITION} || '';

	next unless $cond eq '' or $cond =~ /^(NON_)?FINAL/;

	if (defined $cond && (@$lower > 1 || $lower->[0] != $code)
	    or @$lower > 1 or $lower->[0] != $code) {
	    $lower_special{$code} = [$lower, $cond];
	}
	if (defined $cond && (@$upper > 1 || $upper->[0] != $code)
	    or @$upper > 1 or $upper->[0] != $code) {
	    $upper_special{$code} = [$upper, $cond];
	}
    }
    close SPCASE;
}

#
# read_unicodedata_file -- read UnicodeData.txt
#
sub read_unicodedata_file {
    open UCD, $unicodedatafile or die "cannot open $unicodedatafile: $!\n";

    @decomp_data = (0);
    @toupper_data = (0);
    @tolower_data = (0);

    my @comp_cand;	# canonical composition candidates
    my %nonstarter;

    while ($_ = UCD::UnicodeData::getline(\*UCD)) {
	my %data = UCD::UnicodeData::parseline($_);
	my $code = $data{CODE};

	# combining class
	if ($data{CLASS} > 0) {
	    $nonstarter{$code} = 1;
	    $canon_class->add($code, $data{CLASS});
	}

	# uppercasing
	if (exists $upper_special{$code} or defined $data{UPPER}) {
	    my $offset = @toupper_data;
	    my @casedata;

	    $upper->add($code, $offset);
	    if (exists $upper_special{$code}) {
		push @casedata, $upper_special{$code};
	    }
	    if (defined $data{UPPER}) {
		push @casedata, $data{UPPER};
	    }
	    push @toupper_data, casemap_data(@casedata);
	}

	# lowercasing
	if (exists $lower_special{$code} or defined $data{LOWER}) {
	    my $offset = @tolower_data;
	    my @casedata;

	    $lower->add($code, $offset);
	    if (exists $lower_special{$code}) {
		push @casedata, $lower_special{$code};
	    }
	    if (defined $data{LOWER}) {
		push @casedata, $data{LOWER};
	    }
	    push @tolower_data, casemap_data(@casedata);
	}

	# composition/decomposition
	if ($data{DECOMP}) {
	    my ($tag, @decomp) = @{$data{DECOMP}};
	    my $offset = @decomp_data;

	    # composition
	    if ($tag eq '' and @decomp > 1 and not exists $exclusions{$code}) {
		# canonical composition candidate
		push @comp_cand, [$code, @decomp];
	    }

	    # decomposition
	    if ($tag ne '') {
		# compatibility decomposition
		$offset |= $DECOMP_COMPAT_BIT;
	    }
	    $decomp->add($code, $offset);
	    push @decomp_data, @decomp;
	    $decomp_data[-1] |= END_BIT;

	}

	# final/nonfinal context
	if ($data{CATEGORY} =~ /L[ult]/) {
	    $casemap_ctx->add($code, $LETTER_BIT);
	} elsif ($data{CATEGORY} eq 'Mn') {
	    $casemap_ctx->add($code, $NSPMARK_BIT);
	}
    }
    close UCD;

    # Eliminate composition candidates whose decomposition starts with
    # a non-starter.
    @comp_cand = grep {not exists $nonstarter{$_->[1]}} @comp_cand;

    @comp_data = ([0, 0, 0]);
    my $last_code = -1;
    my $last_offset = @comp_data;
    for my $r (sort {$a->[1] <=> $b->[1] || $a->[2] <=> $b->[2]} @comp_cand) {
	if ($r->[1] != $last_code) {
	    $comp->add($last_code,
		       ($last_offset | ((@comp_data - $last_offset)<<16)))
		unless $last_code == -1;
	    $last_code = $r->[1];
	    $last_offset = @comp_data;
	}
	push @comp_data, $r;
    }
    $comp->add($last_code,
	       ($last_offset | ((@comp_data - $last_offset)<<16)));
}

sub casemap_data {
    my @data = @_;
    my @result = ();
    while (@data > 0) {
	my $r = shift @data;
	my $flag = 0;
	if (ref $r) {
	    if ($r->[1] eq 'FINAL') {
		$flag |= $CASEMAP_FINAL_BIT;
	    } elsif ($r->[1] eq 'NON_FINAL') {
		$flag |= $CASEMAP_NONFINAL_BIT;
	    } elsif ($r->[1] ne '') {
		die "unknown condition \"", $r->[1], "\"\n";
	    }
	}
	$flag |= $CASEMAP_LAST_BIT if @data == 0;
	push @result, $flag;
	push @result, (ref $r) ? @{$r->[0]} : $r;
	$result[-1] |= END_BIT;
    }
    @result;
}

#
# read_casefolding_file -- read CaseFolding.txt
#
sub read_casefolding_file {
    open FOLD, $casefoldingfile or die "cannto open $casefoldingfile: $!\n";

    # dummy.
    @folding_data = (0);

    while ($_ = UCD::CaseFolding::getline(\*FOLD)) {
	my %data = UCD::CaseFolding::parseline($_);

	$folding->add($data{CODE}, scalar(@folding_data));
	push @folding_data, @{$data{MAP}};
	$folding_data[-1] |= END_BIT;
    }
    close FOLD;
}

sub print_header {
    print <<"END";
/* \$Id\$ */
/* $myid */
/*
 * Do not edit this file!
 * This file is generated from UnicodeData.txt, CompositionExclusions-1.txt,
 * SpecialCasing.txt and CaseFolding.txt.
 */

END
}

#
# print_canon_class -- generate data for canonical class
#
sub print_canon_class {
    $canon_class->fix();
    print STDERR "** cannon_class\n", $canon_class->stat() if $verbose;

    print <<"END";

/*
 * Canonical Class
 */

END
    print_bits("CANON_CLASS", @canon_class_bits);
    print "\n";
    print $canon_class->cprog(NAME => "${prefix}canon_class");
}

#
# print_composition -- generate data for canonical composition
#
sub print_composition {
    $comp->fix();
    print STDERR "** composition\n", $comp->stat() if $verbose;

    print <<"END";

/*
 * Canonical Composition
 */

END
    print_bits("CANON_COMPOSE", @comp_bits);
    print "\n";
    print $comp->cprog(NAME => "${prefix}compose");
    print <<"END";

static const struct composition ${prefix}compose_seq[] = {
END
    my $i = 0;
    foreach my $r (@comp_data) {
	if ($i % 2 == 0) {
	    print "\n" if $i != 0;
	    print "\t";
	}
	printf "{ 0x%08x, 0x%08x }, ", $r->[2], $r->[0];
	$i++;
    }
    print "\n};\n\n";
}

#
# print_decomposition -- generate data for canonical/compatibility
# decomposition
#
sub print_decomposition {
    $decomp->fix();
    print STDERR "** decomposition\n", $decomp->stat() if $verbose;

    print <<"END";

/*
 * Canonical/Compatibility Decomposition
 */

END
    print_bits("DECOMP", @decomp_bits);
    print "#define DECOMP_COMPAT\t$DECOMP_COMPAT_BIT\n\n";

    print $decomp->cprog(NAME => "${prefix}decompose");

    print "static const unsigned long ${prefix}decompose_seq[] = {\n";
    print_ulseq(@decomp_data);
    print "};\n\n";
}

#
# print_casemap -- generate data for case mapping
#
sub print_casemap {
    $upper->fix();
    $lower->fix();
    print STDERR "** upper mapping\n", $upper->stat() if $verbose;
    print STDERR "** lower mapping\n", $lower->stat() if $verbose;

    print <<"END";

/*
 * Lowercase <-> Uppercase mapping
 */

/*
 * Flags for special case mapping.
 */
#define CMF_FINAL	$CASEMAP_FINAL_BIT
#define CMF_NONFINAL	$CASEMAP_NONFINAL_BIT
#define CMF_LAST	$CASEMAP_LAST_BIT
#define CMF_CTXDEP	(CMF_FINAL|CMF_NONFINAL)

END
    print_bits("CASEMAP", @casemap_bits);
    print "\n";
    print $upper->cprog(NAME => "${prefix}toupper");
    print $lower->cprog(NAME => "${prefix}tolower");

    print "static const unsigned long ${prefix}toupper_seq[] = {\n";
    print_ulseq(@toupper_data);
    print "};\n\n";

    print "static const unsigned long ${prefix}tolower_seq[] = {\n";
    print_ulseq(@tolower_data);
    print "};\n\n";
}

#
# print_casefolding -- generate data for case folding
#
sub print_casefolding {
    $folding->fix();
    print STDERR "** case folding\n", $folding->stat() if $verbose;

    print <<"END";

/*
 * Case Folding
 */

END
    print_bits("CASE_FOLDING", @folding_bits);
    print "\n";
    print $folding->cprog(NAME => "${prefix}case_folding");

    print "static const unsigned long ${prefix}case_folding_seq[] = {\n";
    print_ulseq(@folding_data);
    print "};\n\n";
}

#
# print_casemap_context -- gerarate data for determining context
# (final/non-final)
#
sub print_casemap_context {
    $casemap_ctx->fix();
    print STDERR "** casemap context\n", $casemap_ctx->stat() if $verbose;

    print <<"END";

/*
 * Cased characters and non-spacing marks (for casemap context)
 */

END

    print_bits("CASEMAP_CTX", @casemap_ctx_bits);
    print <<"END";

#define CTX_CASED	$LETTER_BIT
#define CTX_NSM		$NSPMARK_BIT

END
    print $casemap_ctx->cprog(NAME => "${prefix}casemap_ctx");
}

sub sprint_composition_hash {
    my $i = 0;
    my $s = '';
    foreach my $r (@_) {
	if ($i % 2 == 0) {
	    $s .= "\n" if $i != 0;
	    $s .= "\t";
	}
	$s .= sprintf "{0x%04x, 0x%04x, 0x%04x}, ", @{$r};
	$i++;
    }
    $s;
}

sub print_bits {
    my $prefix = shift;
    my $i = 0;
    foreach my $bit (@_) {
	print "#define ${prefix}_BITS_$i\t$bit\n";
	$i++;
    }
}

sub print_ulseq {
    my $i = 0;
    foreach my $v (@_) {
	if ($i % 4 == 0) {
	    print "\n" if $i != 0;
	    print "\t";
	}
	printf "0x%08x, ", $v;
	$i++;
    }
    print "\n";
}
