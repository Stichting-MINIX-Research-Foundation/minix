#!/usr/bin/perl
#
# Copyright (c) 2010 Kungliga Tekniska Högskolan
# (Royal Institute of Technology, Stockholm, Sweden).
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the Institute nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

require 'getopts.pl';

my $output;
my $CFILE, $HFILE;
my $onlybase;
my $header = 0;

Getopts('b:h') || die "foo";

if($opt_b) {
    $onlybase = $opt_b;
}

$header = 1 if ($opt_h);

printf "/* Generated file */\n";
if ($header) {
    printf "#ifndef GSSAPI_GSSAPI_OID\n";
    printf "#define GSSAPI_GSSAPI_OID 1\n\n";
} else {
    printf "#include \"mech_locl.h\"\n\n";
}

my %tables;
my %types;

while(<>) {

    if (/^\w*#(.*)/) {
	my $comment = $1;

	if ($header) {
	    printf("$comment\n");
	}

    } elsif (/^oid\s+([\w\.]+)\s+(\w+)\s+([\w\.]+)/) {
	my ($base, $name, $oid) = ($1, $2, $3);

	next if (defined $onlybase and $onlybase ne $base);

	my $store = "__" . lc($name) . "_oid_desc";

	# encode oid

	my @array = split(/\./, $oid);
	my $length = 0;
	my $data = "";

	my $num;

	$n = $#array;
	while ($n > 1) {
	    $num = $array[$n];

	    my $p = int($num % 128);
	    $data = sprintf("\\x%02x", $p) . $data;

	    $num = int($num / 128);

	    $length += 1;

	    while ($num > 0) {
		$p = int($num % 128) + 128;
		$num = int($num / 128);
		$data = sprintf("\\x%02x", $p) . $data;
		$length += 1;
	    }
	    $n--;
	}
	$num = int($array[0] * 40 + $array[1]);

	$data = sprintf("\\x%x", $num) . $data;
	$length += 1;

	if ($header) {
	    printf "extern GSSAPI_LIB_VARIABLE gss_OID_desc $store;\n";
	    printf "#define $name (&$store)\n\n";
	} else {
	    printf "/* $name - $oid */\n";
	    printf "gss_OID_desc GSSAPI_LIB_VARIABLE $store = { $length, rk_UNCONST(\"$data\") };\n\n";
	}
    } elsif (/^desc\s+([\w]+)\s+(\w+)\s+(\"[^\"]*\")\s+(\"[^\"]*\")/) {
        my ($type, $oid, $short, $long) = ($1, $2, $3, $4);
	my $object = { type=> $type, oid => $oid, short => $short, long => $long };
	
	$tables{$oid} = \$object;
	$types{$type} = 1;
    }

}

foreach my $k (keys %types) {
    if (!$header) {
	print "struct _gss_oid_name_table _gss_ont_" . $k . "[] = {\n";
	foreach my $m (values %tables) {
	    if ($$m->{type} eq $k) {
		printf "  { %s, \"%s\", %s, %s },\n", $$m->{oid}, $$m->{oid}, $$m->{short}, $$m->{long};
	    }
	}
	printf "  { NULL }\n";
	printf "};\n\n";
	
    }
}

if ($header) {
    printf "#endif /* GSSAPI_GSSAPI_OID */\n";
}
