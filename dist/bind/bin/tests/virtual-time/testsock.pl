#!/usr/bin/perl
#
# Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: testsock.pl,v 1.2 2010-06-17 05:38:05 marka Exp $

# Test whether the interfaces on 10.53.0.* are up.

require 5.001;

use Socket;
use Getopt::Long;

my $port = 0;
my $id = 0;
GetOptions("p=i" => \$port,
           "i=i" => \$id);

my @ids;
if ($id != 0) {
	@ids = ($id);
} else {
	@ids = (1..5);
}

foreach $id (@ids) {
        my $addr = pack("C4", 10, 53, 0, $id);
	my $sa = pack_sockaddr_in($port, $addr);
	socket(SOCK, PF_INET, SOCK_STREAM, getprotobyname("tcp"))
      		or die "$0: socket: $!\n";
	setsockopt(SOCK, SOL_SOCKET, SO_REUSEADDR, pack("l", 1));

	bind(SOCK, $sa)
	    	or die sprintf("$0: bind(%s, %d): $!\n",
			       inet_ntoa($addr), $port);
	close(SOCK);
	sleep(1);
}
