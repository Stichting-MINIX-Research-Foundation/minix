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

my $show_module_name = 1;
my $use_indent = 1;
my $strip_leading_underscore = 0;

# Dump all symbols for the given object file that are defined and have
# external scope.

sub dump_symbols_for_file($)
{
    $fn = shift;

    print STDERR "Opening dump of object [$fn]\n";

    open(SP, '-|', "dumpbin /symbols \"".$fn."\"") or die "Can't open pipe for $fn";

  LINE:
    while (<SP>) {
	# 008 00000000 SECT3  notype ()    External     | _encode_AccessDescription

	/^[[:xdigit:]]{3,}\s[[:xdigit:]]{8,}\s(\w+)\s+\w*\s+(\(\)|  )\s+(\w+)\s+\|\s+([0-9a-zA-Z\@\_]+)$/ && do {
	    my ($section, $type, $visibility, $symbol) = ($1, $2, $3, $4);

	    if ($section ne "UNDEF" && $visibility eq "External") {
		print $fn if $show_module_name;
		print "\t" if $use_indent || $show_module_name;

		if ($strip_leading_underscore && $symbol =~ /_(.*)/) {
		    $symbol = $1;
		}
		if ($strip_leading_underscore && $symbol =~ /(.*)\@.*$/) {
		    $symbol = $1;
		}
		print $symbol;
                if ($type ne "()") {
                    print "\tDATA";
                }
		print "\n";
	    }
	};
    }

    close SP;
}

sub use_response_file($)
{
    $fn = shift;

    open (RF, '<', $fn) or die "Can't open response file $fn";

    while (<RF>) {
	/(\S+)/ && do {
	    dump_symbols_for_file($1);
	}
    }
    close RF;
}

for (@ARGV) {
    ARG: {
	/^-q$/ && do {
	    $show_module_name = 0;
	    last ARG;
	};

	/^-1$/ && do {
	    $use_indent = 0;
	    last ARG;
	};

	/^-u$/ && do {
	    $strip_leading_underscore = 1;
	    last ARG;
	};

	/^@(.*)$/ && do {
	    use_response_file($1);
	    last ARG;
	};

	dump_symbols_for_file($_);
    }
}
