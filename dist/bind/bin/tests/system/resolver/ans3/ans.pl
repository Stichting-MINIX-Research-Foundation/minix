#!/usr/bin/perl
#
# Copyright (C) 2004, 2007, 2009  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

# $Id: ans.pl,v 1.12 2009-11-04 02:15:30 marka Exp $

#
# Ad hoc name server
#

use IO::File;
use IO::Socket;
use Net::DNS;
use Net::DNS::Packet;

my $sock = IO::Socket::INET->new(LocalAddr => "10.53.0.3",
   LocalPort => 5300, Proto => "udp") or die "$!";

my $pidf = new IO::File "ans.pid", "w" or die "cannot open pid file: $!";
print $pidf "$$\n" or die "cannot write pid file: $!";
$pidf->close or die "cannot close pid file: $!";
sub rmpid { unlink "ans.pid"; exit 1; };

$SIG{INT} = \&rmpid;
$SIG{TERM} = \&rmpid;

for (;;) {
	$sock->recv($buf, 512);

	print "**** request from " , $sock->peerhost, " port ", $sock->peerport, "\n";

	my ($packet, $err) = new Net::DNS::Packet(\$buf, 0);
	$err and die $err;

	print "REQUEST:\n";
	$packet->print;

	$packet->header->qr(1);
	$packet->header->aa(1);

	my @questions = $packet->question;
	my $qname = $questions[0]->qname;

	if ($qname eq "badcname.example.net") {
		$packet->push("answer",
			      new Net::DNS::RR($qname .
				       " 300 CNAME badcname.example.org"));
	} elsif ($qname eq "foo.baddname.example.net") {
		$packet->push("answer",
			      new Net::DNS::RR("baddname.example.net" .
				       " 300 DNAME baddname.example.org"));
	} elsif ($qname eq "foo.gooddname.example.net") {
		$packet->push("answer",
			      new Net::DNS::RR("gooddname.example.net" .
				       " 300 DNAME gooddname.example.org"));
	} elsif ($qname eq "goodcname.example.net") {
		$packet->push("answer",
			      new Net::DNS::RR($qname .
				       " 300 CNAME goodcname.example.org"));
	} elsif ($qname eq "cname.sub.example.org") {
		$packet->push("answer",
			      new Net::DNS::RR($qname .
				       " 300 CNAME ok.sub.example.org"));
	} elsif ($qname eq "ok.sub.example.org") {
		$packet->push("answer",
			      new Net::DNS::RR($qname . " 300 A 192.0.2.1"));
	} elsif ($qname eq "www.dname.sub.example.org") {
		$packet->push("answer",
			      new Net::DNS::RR("dname.sub.example.org" .
				       " 300 DNAME ok.sub.example.org"));
	} elsif ($qname eq "www.ok.sub.example.org") {
		$packet->push("answer",
			      new Net::DNS::RR($qname . " 300 A 192.0.2.1"));
	} else {
		$packet->push("answer", new Net::DNS::RR("www.example.com 300 A 1.2.3.4"));
	}

	$sock->send($packet->data);

	print "RESPONSE:\n";
	$packet->print;
	print "\n";
}
