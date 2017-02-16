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

# Id: packet.pl,v 1.2 2011/04/15 01:02:08 each Exp 

# This is a tool for sending an arbitrary packet via UDP or TCP to an
# arbitrary address and port.  The packet is specified in a file or on
# the standard input, in the form of a series of bytes in hexidecimal.
# Whitespace is ignored, as is anything following a '#' symbol.
#
# For example, the following input would generate normal query for 
# isc.org/NS/IN":
#
#     # QID:
#     0c d8
#     # header:
#     01 00 00 01 00 00 00 00 00 00
#     # qname isc.org:
#     03 69 73 63 03 6f 72 67 00
#     # qtype NS:
#     00 02
#     # qclass IN:
#     00 01
#
# Note that we do not wait for a response for the server.  This is simply
# a way of injecting arbitrary packets to test server resposnes.
#
# Usage: packet.pl [-a <address>] [-p <port>] [-t (udp|tcp)] [filename]
#
# If not specified, address defaults to 127.0.0.1, port to 53, protocol
# to udp, and file to stdin.
#
# XXX: Doesn't support IPv6 yet

require 5.006.001;

use strict;
use Getopt::Std;
use IO::File;
use IO::Socket;

sub usage {
    print ("Usage: packet.pl [-a address] [-p port] [-t (tcp|udp)] [file]\n");
    exit 1;
}

my %options={};
getopts("a:p:t:", \%options);

my $addr = "127.0.0.1";
$addr = $options{a} if defined $options{a};

my $port = 53;
$port = $options{p} if defined $options{p};

my $proto = "udp";
$proto = lc $options{t} if defined $options{t};
usage if ($proto !~ /^(udp|tcp)$/);

my $file = "STDIN";
if (@ARGV >= 1) {
    my $filename = shift @ARGV;
    open FH, "<$filename" or die "$filename: $!";
    $file = "FH";
}

my $input = "";
while (defined(my $line = <$file>) ) {
    chomp $line;
    $line =~ s/#.*$//;
    $input .= $line;
}

$input =~ s/\s+//g;
my $data = pack("H*", $input);
my $len = length $data;

my $output = unpack("H*", $data);
print ("sending: $output\n");

my $sock = IO::Socket::INET->new(PeerAddr => $addr, PeerPort => $port,
				 Proto => $proto,) or die "$!";

my $bytes;
if ($proto eq "udp") {
    $bytes = $sock->send($data);
} else {
    $bytes = $sock->syswrite(pack("n", $len), 2);
    $bytes += $sock->syswrite($data, $len);
}

print ("sent $bytes bytes to $addr:$port\n");
$sock->close;
close $file;
