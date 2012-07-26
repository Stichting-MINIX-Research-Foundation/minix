# $Id: UCD.pm,v 1.1.1.1 2003-06-04 00:27:53 marka Exp $
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

package UCD;

#
# UCD.pm -- parser for Unicode Character Database files.
#
# This file is an aggregation of the following modules, each of which
# provides a parser for a specific data file of UCD.
#	UCD::UnicodeData		-- for UnicodeData.txt
#	UCD::CaseFolding		-- for CaseFolding.txt
#	UCD::SpecialCasing		-- for SpecialCasing.txt
#	UCD::CompositionExclusions	-- for CompositionExclusions-1.txt
#
# Each module provides two subroutines:
#
#   $line = getline(\*HANDLE);
#	reads next non-comment line from HANDLE, and returns it.
#	undef will be returned upon EOF.
#
#   %fields = parse($line);
#	parses a line and extract fields, and returns a list of
#	field name and its value, suitable for assignment to a hash.
#

package UCD::UnicodeData;

use strict;
use Carp;

sub getline {
    my $fh = shift;
    my $s = <$fh>;
    $s =~ s/\r?\n$// if $s;
    $s;
}

sub parseline {
    my $s = shift;

    my @f = split /;/, $s, -1;
    return (CODE     => hex($f[0]),
	    NAME     => $f[1],
	    CATEGORY => $f[2],
	    CLASS    => $f[3]+0,
	    BIDI     => $f[4],
	    DECOMP   => dcmap($f[5]),
	    DECIMAL  => dvalue($f[6]),
	    DIGIT    => dvalue($f[7]),
	    NUMERIC  => dvalue($f[8]),
	    MIRRORED => $f[9] eq 'Y',
	    NAME10   => $f[10],
	    COMMENT  => $f[11],
	    UPPER    => ucode($f[12]),
	    LOWER    => ucode($f[13]),
	    TITLE    => ucode($f[14]));
}

sub dcmap {
    my $v = shift;
    return undef if $v eq '';
    $v =~ /^(?:(<[^>]+>)\s*)?(\S.*)/
	or croak "invalid decomposition mapping \"$v\"";
    my $tag = $1 || '';
    [$tag, map {hex($_)} split(' ', $2)];
}

sub ucode {
    my $v = shift;
    return undef if $v eq '';
    hex($v);
}

sub dvalue {
    my $v = shift;
    return undef if $v eq '';
    $v;
}

package UCD::CaseFolding;

use strict;

sub getline {
    my $fh = shift;
    while (defined(my $s = <$fh>)) {
	next if $s =~ /^\#/;
	next if $s =~ /^\s*$/;
	$s =~ s/\r?\n$//;
	return $s;
    }
    undef;
}

sub parseline {
    my $s = shift;
    my @f = split /;\s*/, $s, -1;
    return (CODE => hex($f[0]),
	    TYPE => $f[1],
	    MAP  => [map(hex, split ' ', $f[2])],
	   );
}

package UCD::SpecialCasing;

use strict;

sub getline {
    my $fh = shift;
    while (defined(my $s = <$fh>)) {
	next if $s =~ /^\#/;
	next if $s =~ /^\s*$/;
	$s =~ s/\r?\n$//;
	return $s;
    }
    undef;
}

sub parseline {
    my $s = shift;

    my @f = split /;\s*/, $s, -1;
    my $cond = (@f > 5) ? $f[4] : undef;
    return (CODE => hex($f[0]),
	    LOWER => [map(hex, split ' ', $f[1])],
	    TITLE => [map(hex, split ' ', $f[2])],
	    UPPER => [map(hex, split ' ', $f[3])],
	    CONDITION => $cond);
}

package UCD::CompositionExclusions;

use strict;

sub getline {
    my $fh = shift;
    while (defined(my $s = <$fh>)) {
	next if $s =~ /^\#/;
	next if $s =~ /^\s*$/;
	$s =~ s/\r?\n$//;
	return $s;
    }
    undef;
}

sub parseline {
    my $s = shift;
    m/^[0-9A-Fa-f]+/;
    return (CODE => hex($&));
}

1;
