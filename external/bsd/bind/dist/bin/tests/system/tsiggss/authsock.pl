#!/usr/bin/env perl
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

# Id: authsock.pl,v 1.3 2011/01/07 23:47:07 tbox Exp 

# test the update-policy external protocol

require 5.6.0;

use IO::Socket::UNIX;
use Getopt::Long;

my $path;
my $typeallowed = "A";
my $pidfile = "authsock.pid";
my $timeout = 0;

GetOptions("path=s" => \$path,
	   "type=s" => \$typeallowed,
	   "pidfile=s" => \$pidfile,
	   "timeout=i" => \$timeout);

if (!defined($path)) {
	print("Usage: authsock.pl --path=<sockpath> --type=type --pidfile=pidfile\n");
	exit(1);
}

unlink($path);
my $server = IO::Socket::UNIX->new(Local => $path, Type => SOCK_STREAM, Listen => 8) or
    die "unable to create socket $path";
chmod 0777, $path;

# setup our pidfile
open(my $pid,">",$pidfile)
    or die "unable to open pidfile $pidfile";
print $pid "$$\n";
close($pid);

if ($timeout != 0) {
    # die after the given timeout
    alarm($timeout);
}

while (my $client = $server->accept()) {
	$client->recv(my $buf, 8, 0);
	my ($version, $req_len) = unpack('N N', $buf);

	if ($version != 1 || $req_len < 17) {
		printf("Badly formatted request\n");
		$client->send(pack('N', 2));
		next;
	}

	$client->recv(my $buf, $req_len - 8, 0);

	my ($signer,
	    $name,
	    $addr,
	    $type,
	    $key,
	    $key_data) = unpack('Z* Z* Z* Z* Z* N/a', $buf);

	if ($req_len != length($buf)+8) {
		printf("Length mismatch %u %u\n", $req_len, length($buf)+8);
		$client->send(pack('N', 2));
		next;
	}

	printf("version=%u signer=%s name=%s addr=%s type=%s key=%s key_data_len=%u\n",
	       $version, $signer, $name, $addr, $type, $key, length($key_data));

	my $result;
	if ($typeallowed eq $type) {
		$result = 1;
		printf("allowed type %s == %s\n", $type, $typeallowed);
	} else {
		printf("disallowed type %s != %s\n", $type, $typeallowed);
		$result = 0;
	}

	$reply = pack('N', $result);
	$client->send($reply);
}
