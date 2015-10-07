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
my $always_export = 0;
my $module_name = "";
my $local_prefix = "SHIM_";
my %forward_exports = ();
my %local_exports = ();

sub build_forwarder_target_list($)
{
    $fn = shift;

    print STDERR "Processing defs from file [$fn]\n";

    open(SP, '-|', "dumpbin /exports \"".$fn."\"") or die "Can't open pipe for $fn";

  LINE:
    while (<SP>) {
#        112   6F 00071CDC krb5_encrypt_size

	/^ +([[:digit:]]+)\s+[[:xdigit:]]+\s[[:xdigit:]]{8,}\s+(\S+)(?:| = (\S*))$/ && do {
	    my ($ordinal, $symbol, $in) = ($1, $2, $3);

	    if ($in eq "") { $in = $symbol };
	    $forward_exports{$symbol} = $in;
	};
    }

    close SP;
}

# Dump all symbols for the given dll file that are defined and have
# external scope.

sub build_def_file($)
{
    $fn = shift;

    print STDERR "Opening dump of DLL [$fn]\n";

    open(SP, '-|', "dumpbin /exports \"".$fn."\"") or die "Can't open pipe for $fn";

  LINE:
    while (<SP>) {
#        112   6F 00071CDC krb5_encrypt_size

	/^ +([[:digit:]]+)\s+[[:xdigit:]]+\s[[:xdigit:]]{8,}\s+(\S+)(?:| = (\S*))$/ && do {
	    my ($ordinal, $symbol, $in) = ($1, $2, $3);

	    if ($strip_leading_underscore && $symbol =~ /_(.*)/) {
		$symbol = $1;
	    }
	    if (exists $local_exports{$symbol}) {
		print "\t".$symbol;
		print " = ".$local_exports{$symbol};
		if ($in ne $local_exports{$symbol} and $in ne "") {
		    print STDERR "Incorrect calling convention for local $symbol\n";
		    print STDERR "  ".$in." != ".$local_exports{$symbol}."\n";
		}
		print "\t@".$ordinal."\n";
	    } elsif (exists $local_exports{$local_prefix.$symbol}) {
		print "\t".$symbol;
		print " = ".$local_exports{$local_prefix.$symbol};
		print "\t@".$ordinal."\n";
	    } elsif (exists $forward_exports{$symbol}) {
		print "\t".$symbol;
		print " = ".$module_name;
		if ($in ne $forward_exports{$symbol} and $in ne "") {
		    print STDERR "Incorrect calling convention for $symbol\n";
		    print STDERR "  ".$in." != ".$forward_exports{$symbol}."\n";
		}
		my $texp = $forward_exports{$symbol};
		if ($texp =~ /^_([^@]+)$/) { $texp = $1; }
		print $texp."\t@".$ordinal."\n";
	    } elsif ($always_export) {
                print "\t".$symbol." = ".$local_prefix.$symbol;
                print "\t@".$ordinal."\n";
            } else {
		print STDERR "Symbol not found: $symbol\n";
	    }
	};
    }

    close SP;
}

sub build_local_exports_list($)
{
    $fn = shift;

    print STDERR "Opening dump of object [$fn]\n";

    open(SP, '-|', "dumpbin /symbols \"".$fn."\"") or die "Can't open pipe for $fn";

  LINE:
    while (<SP>) {
	# 009 00000010 SECT3  notype ()    External     | _remove_error_table@4
	m/^[[:xdigit:]]{3,}\s[[:xdigit:]]{8,}\s(\w+)\s+\w*\s+(?:\(\)|  )\s+(\w+)\s+\|\s+(\S+)$/ && do {
	    my ($section, $visibility, $symbol) = ($1, $2, $3);

	    if ($section ne "UNDEF" && $visibility eq "External") {

		my $exp_name = $symbol;

		if ($symbol =~ m/^_(\w+)(?:@.*|)$/) {
		    $exp_name = $1;
		}

		if ($symbol =~ m/^_([^@]+)$/) {
		    $symbol = $1;
		}

		$local_exports{$exp_name} = $symbol;
	    }
	};
    }

    close SP;
}

sub process_file($)
{
    $fn = shift;

    if ($fn =~ m/\.dll$/i) {
	build_def_file($fn);
    } elsif ($fn =~ m/\.obj$/i) {
	build_local_exports_list($fn);
    } else {
	die "File type not recognized for $fn.";
    }
}

sub use_response_file($)
{
    $fn = shift;

    open (RF, '<', $fn) or die "Can't open response file $fn";

    while (<RF>) {
	/^(\S+)$/ && do {
	    process_file($1);
	}
    }
    close RF;
}

print "; This is a generated file.  Do not modify directly.\n";
print "EXPORTS\n";

for (@ARGV) {
    ARG: {
	/^-m(.*)$/ && do {
	    $module_name = $1.".";
	    last ARG;
	};

        /^-l(.*)$/ && do {
            $local_prefix = $1."_";
            last ARG;
        };

        /^-a$/ && do {
            $always_export = 1;
            last ARG;
        };

	/^-e(.*)$/ && do {
	    build_forwarder_target_list($1);
	    last ARG;
	};

	/^@(.*)$/ && do {
	    use_response_file($1);
	    last ARG;
	};

	process_file($_);
    }
}
