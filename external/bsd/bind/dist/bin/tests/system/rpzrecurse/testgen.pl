#!/usr/bin/env perl
#
# Copyright (C) 2015  Internet Systems Consortium, Inc. ("ISC")
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

use strict;
use warnings;

my $boilerplate_header = <<'EOB';
# common configuration
include "named.conf.header";

view "recursive" {
    zone "." {
        type hint;
        file "root.hint";
    };

    # policy configuration to be tested
    response-policy {
EOB

my $no_option = <<'EOB';
    };

    # policy zones to be tested
EOB

my $qname_wait_recurse = <<'EOB';
    } qname-wait-recurse no;

    # policy zones to be tested
EOB

my $boilerplate_end = <<'EOB';
};
EOB

my $policy_option = $qname_wait_recurse;

my $serialnum = "1";
my $policy_zone_header = <<'EOH';
$TTL 60
@ IN SOA root.ns ns SERIAL 3600 1800 86400 60
     NS ns
ns A 127.0.0.1
EOH

sub policy_client_ip {
    return "32.1.0.0.127.rpz-client-ip CNAME .\n";
}

sub policy_qname {
    my $query_nbr = shift;
    return sprintf "q%02d.l2.l1.l0 CNAME .\n", $query_nbr;
}

sub policy_ip {
    return "32.255.255.255.255.rpz-ip CNAME .\n";
}

sub policy_nsdname {
    return "ns.example.org.rpz-nsdname CNAME .\n";
}

sub policy_nsip {
    return "32.255.255.255.255.rpz-ip CNAME .\n";
}

my %static_triggers = (
    'client-ip' => \&policy_client_ip,
    'ip'        => \&policy_ip,
    'nsdname'   => \&policy_nsdname,
    'nsip'      => \&policy_nsip,
);

sub mkconf {
    my $case_id = shift;
    my $n_queries = shift;

    { # generate the query list
        my $query_list_filename = "ns2/$case_id.queries";
        my $query_list_fh;

        open $query_list_fh, ">$query_list_filename" or die;

        for( my $i = 1; $i <= $n_queries; $i++ ) {
            print $query_list_fh sprintf "q%02d.l2.l1.l0\n", $i;
        }
    }

    my @zones;

    { # generate the conf file
        my $conf_filename = "ns2/named.$case_id.conf";

        my $conf_fh;

        open $conf_fh, ">$conf_filename" or die;

        print $conf_fh $boilerplate_header;

        my $zone_seq = 0;

        @zones = map {
            [
                sprintf( "$case_id.%02d.policy.local", $zone_seq++ ),
                $_,
            ];
        } @_;

        print $conf_fh map { qq{        zone "$_->[0]";\n} } @zones;

        print $conf_fh $policy_option;

        print $conf_fh map { qq{    zone "$_->[0]" { type master; file "db.$_->[0]"; };\n} } @zones;

        print $conf_fh $boilerplate_end;
    }

    # generate the policy zone contents
    foreach my $policy_zone_info( @zones ) {
        my $policy_zone_name = $policy_zone_info->[0];
        my $policy_zone_contents = $policy_zone_info->[1];

        my $policy_zone_filename = "ns2/db.$policy_zone_name";
        my $policy_zone_fh;

        open $policy_zone_fh, ">$policy_zone_filename" or die;

        my $header = $policy_zone_header;
        $header =~ s/SERIAL/$serialnum/;
        print $policy_zone_fh $header;

        foreach my $trigger( @$policy_zone_contents ) {
            if( exists $static_triggers{$trigger} ) {
                # matches a trigger type with a static value
                print $policy_zone_fh $static_triggers{$trigger}->();
            }
            else {
                # a qname trigger, where what was specified is the query number it should match
                print $policy_zone_fh policy_qname( $trigger );
            }
        }
    }
}

mkconf(
    '1a',
    1,
    [ 'client-ip' ],
);

mkconf(
    '1b',
    2,
    [ 1 ],
);

mkconf(
    '1c',
    1,
    [ 'client-ip', 2 ],
);

mkconf(
    '2a',
    33,
    map { [ $_ ]; }  1 .. 32
);

mkconf(
    '3a',
    1,
    [ 'ip' ],
);

mkconf(
    '3b',
    1,
    [ 'nsdname' ],
);

mkconf(
    '3c',
    1,
    [ 'nsip' ],
);

mkconf(
    '3d',
    2,
    [ 'ip', 1 ]
);

mkconf(
    '3e',
    2,
    [ 'nsdname', 1 ]
);

mkconf(
    '3f',
    2,
    [ 'nsip', 1 ]
);

{
    my $seq_code = 'aa';
    my $seq_nbr = 0;

    while( $seq_nbr < 32 ) {

        mkconf(
            "4$seq_code",
            33,
            ( map { [ $_ ]; } 1 .. $seq_nbr ),
            [ 'ip', $seq_nbr + 2 ],
            ( map { [ $_ + 2 ]; } ($seq_nbr + 1) .. 31 ),
        );

        $seq_code++;
        $seq_nbr++;
    }
}

mkconf(
    '5a',
    6,
    [ 1 ],
    [ 2, 'ip' ],
    [ 4 ],
    [ 5, 'ip' ],
    [ 6 ],
);

$policy_option = $no_option;

mkconf(
    '6a',
    0,
    [ ],
);

$serialnum = "2";
mkconf(
    '6b',
    0,
    [ 'nsdname' ],
);

$serialnum = "3";
mkconf(
    '6c',
    0,
    [ ],
);

__END__

0x01 - has client-ip
    32.1.0.0.127.rpz-client-ip CNAME .
0x02 - has qname
    qX.l2.l1.l0 CNAME .
0x10 - has ip
    32.255.255.255.255.rpz-ip CNAME .
0x20 - has nsdname
    ns.example.org.rpz-nsdname CNAME .
0x40 - has nsip
    32.255.255.255.255.rpz-nsip CNAME .

$case.$seq.policy.local

case 1a = 0x01
    .q01 = (00,0x01)=-r
case 1b = 0x02
    .q01 = (00,0x02)=-r
    .q02 = (--,----)=+r
case 1c = 0x03
    .q01 = (00,0x01)=-r

case 2a = 0x03{32}
    .q01 = (00,0x02)=-r
    .q02 = (01,0x02)=-r
     ...
    .q31 = (30,0x02)=-r
    .q32 = (31,0x02)=-r
    .q33 = (--,----)=+r

case 3a = 0x10
    .q01 = (00,0x10)=+r
case 3b = 0x20
    .q01 = (00,0x20)=+r
case 3c = 0x40
    .q01 = (00,0x40)=+r
case 3d = 0x12
    .q01 = (00,0x10)=+r
    .q02 = (00,0x02)=-r
case 3e = 0x22
    .q01 = (00,0x20)=+r
    .q02 = (00,0x02)=-r
case 3f = 0x42
    .q01 = (00,0x40)=+r
    .q02 = (00,0x02)=-r

case 4aa = 0x12,0x02{31}
    .q01 = (00,0x10)=+r
    .q02 = (00,0x02)=-r
    .q03 = (01,0x02)=+r
     ...
    .q32 = (30,0x02)=+r
    .q33 = (31,0x02)=+r
case 4__ = 0x02{n(1->30)},0x12,0x02{31-n}
    .q01 = (00,0x02)=-r
     ...
    .q(n+1) = (n,0x10)=+r
    .q(n+2) = (n,0x02)=-r
     ...
    .q33 = (31,0x02)=+r
case 4bf = 0x02{31},0x12
    .q01 = (00,0x02)=-r
    .q02 = (01,0x02)=-r
     ...
    .q31 = (30,0x02)=-r
    .q32 = (31,0x10)=+r
    .q33 = (31,0x02)=-r

case 5a = 0x02,0x12,0x02,0x12,0x02
    .q01 = (00,0x02)=-r
    .q02 = (01,0x02)=-r
    .q03 = (01,0x10)=+r
    .q04 = (02,0x02)=+r
    .q05 = (03,0x02)=+r
    .q06 = (04,0x02)=+r

