########################################################################
#
# Copyright (c) 2010, Secure Endpoints Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

use Getopt::Long;
use Pod::Usage;
use feature "switch";

my $def_name = '';
my $vs_name = '';
my $show_help = 0;

my %syms;

my $def_only = 0;
my $vs_only = 0;

GetOptions ("def=s" => \$def_name,
	    "vs=s" => \$vs_name,
	    "help|?" => \$show_help) or pod2usage( -exitval => 2,
						   -verbose => 3 );
pod2usage( -exitval => 1,
	   -verbose => 3 ) if $show_help or !$def_name or !$vs_name;

open (my $def, '<', $def_name) or die $!;
open (my $vs, '<', $vs_name) or die $!;

# First go through the version-script

my $global = 0;

while(<$vs>)
{
    next unless m/^([^#]+)/;

    @a = split(/\s+|({|})/,$1);

    for $f (@a) {
	given ($f) {
	    when (/global\:/) { $global = 1; }
	    when (/{|}|.*\:/) { $global = 0; }
	    when (/(.*)\;/ and $global == 1) {
		$syms{$1} = 1;
	    }
	}
    }
}

while(<$def>)
{
    next if m/^#/;
    next unless m/^;!([^;]+)/ or m/^([^;]+);?(!?)/;

    @a = split(/\s+/, $1);

    for $f (@a) {
	next if $f =~ /EXPORTS/ or $f =~ /DATA/ or not $f;

	if (not exists $syms{$f} and not $2) {
	    print "$f: Only in DEF\n";
	    ++$def_only;
	}
	delete $syms{$f};
    }
}

#while (($k,$v) = each %syms) {
for $k (sort keys %syms) {
    print "$k: Only in VS\n";
    ++$vs_only;
}

close($def);
close($vs);

if ($def_only or $vs_only) {
    print "\nMismatches found.\n";
    exit(1);
}

__END__

=head1 NAME

w32-sync-exported-symbols.pl - Synchronize Windows .def with version-script

=head1 SYNOPSIS

w32-sync-exported-symbols.pl {options}

  Options:
    --def        Name of .def file
    --vs         Name of version-script file

=head1 DESCRIPTION

Verifies that all the symbols exported by the version-script is also
accounted for in the .def file.  Also checks that no extra symbols are
exported by the .def file unless they are marked as safe.

=cut

