#
# Copyright (C) 2009-2012  John Eaglesham
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND JOHN EAGLESHAM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# JOHN EAGLESHAM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

package dlz_perl_example;

use warnings;
use strict;

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

# Constructor. Everything after the class name can be folded into a hash of
# various options and settings. Right now only log_context and argv are
# available.
sub new {
    my ( $class, %config ) = @_;
    my $self = {};
    bless $self, $class;

    $self->{log} = sub {
        my ( $level, $msg ) = @_;
        DLZ_Perl::log( $config{log_context}, $level, $msg );
    };

    if ( $config{argv} ) { warn "Got argv: $config{argv}\n"; }

    $self->{zones} = {
        'example.com' => {
            '@' => [
                {
                    type => 'SOA',
                    ttl  => 86400,
                    data =>
                     'ns1.example.com. hostmaster.example.com. 12345 172800 900 1209600 3600',
                }
            ],
            perlrr => [
                {
                    type => 'A',
                    ttl  => 444,
                    data => '1.1.1.1',
                },
                {
                    type => 'A',
                    ttl  => 444,
                    data => '1.1.1.2',
                }
            ],
            perltime => [
                {
                    code => sub {
                        return ['TXT', '1', time()];
                    },
                },
            ],
            sourceip => [
                {
                    code => sub {
                        my ( $opaque ) = @_;
                        # Passing anything other than the proper opaque value,
                        # 0, or undef to this function will cause a crash (at
                        # best!).
                        my ( $addr, $port ) =
                         DLZ_Perl::clientinfo::sourceip( $opaque );
                        if ( !$addr ) { $addr = $port = 'unknown'; }
                        return ['TXT', '1', $addr], ['TXT', '1', $port];
                    },
                },
            ],
        },
    };

    $self->{log}->(
        DLZ_Perl::LOG_INFO(),
        'DLZ Perl Script: Called init. Loaded zone data: '
         . Dumper( $self->{zones} )
    );
    return $self;
}

# Do we have data for this zone? Expects a simple true or false return value.
sub findzone {
    my ( $self, $zone ) = @_;
    $self->{log}->(
        DLZ_Perl::LOG_INFO(),
        "DLZ Perl Script: Called findzone, looking for zone $zone"
    );

    return exists $self->{zones}->{$zone};
}

# Return the data for a given record in a given zone. The final parameter is
# an opaque value that can be passed to DLZ_Perl::clientinfo::sourceip to
# retrieve the client source IP and port. Expected return value is an array
# of array refs, with each array ref representing one record and containing
# the type, ttl, and data in that order. Data is as it appears in a zone file.
sub lookup {
    my ( $self, $name, $zone, $client_info ) = @_;
    $self->{log}->(
        DLZ_Perl::LOG_INFO(),
        "DLZ Perl Script: Called lookup, looking for record $name in zone $zone"
    );
    return unless $self->{zones}->{$zone}->{$name};

    my @results;
    foreach my $rr ( @{ $self->{zones}->{$zone}->{$name} } ) {
        if ( $rr->{'code'} ) {
            my @r = $rr->{'code'}->( $client_info );
            if ( @r ) {
                push @results, @r;
            }
        } else {
            push @results, [$rr->{'type'}, $rr->{'ttl'}, $rr->{'data'}];
        }
    }

    return @results;
}

# Will we allow zone transfer for this client? Expects a simple true or false
# return value.
sub allowzonexfr {
    my ( $self, $zone, $client ) = @_;
    $self->{log}->(
        DLZ_Perl::LOG_INFO(),
        "DLZ Perl Script: Called allowzonexfr, looking for zone $zone for " .
        "client $client"
    );
    if ( $client eq '127.0.0.1' ) { return 1; }
    return 0;
}

# Note the return AoA for this method differs from lookup in that it must
# return the name of the record as well as the other data.
sub allnodes {
    my ( $self, $zone ) = @_;
    my @results;
    $self->{log}->(
        DLZ_Perl::LOG_INFO(),
        "DLZ Perl Script: Called allnodes, looking for zone $zone"
    );

    foreach my $name ( keys %{ $self->{zones}->{$zone} } ) {
        foreach my $rr ( @{ $self->{zones}->{$zone}->{$name} } ) {
            if ( $rr->{'code'} ) {
                my @r = $rr->{'code'}->();
                # The code returns an array of array refs without the name.
                # This makes things easy for lookup but hard here. We must
                # iterate over each array ref and inject the name into it.
                foreach my $a ( @r ) {
                    unshift @{$a}, $name;
                }
                push @results, @r;
            } else {
                push @results,
                 [$name, $rr->{'type'}, $rr->{'ttl'}, $rr->{'data'}];
            }
        }
    }
    return @results;
}

1;
