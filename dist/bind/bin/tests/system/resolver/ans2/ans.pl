#!/usr/bin/perl
#
# Copyright (C) 2004, 2007, 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: ans.pl,v 1.15 2010-05-19 09:33:50 tbox Exp $

#
# Ad hoc name server
#

use IO::File;
use IO::Socket;
use Net::DNS;
use Net::DNS::Packet;

my $sock = IO::Socket::INET->new(LocalAddr => "10.53.0.2",
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

	my @questions = $packet->question;
	my $qname = $questions[0]->qname;
	my $qtype = $questions[0]->qtype;

	if ($qname eq "cname1.example.com") {
		# Data for the "cname + other data / 1" test
		$packet->push("answer", new Net::DNS::RR("cname1.example.com 300 CNAME cname1.example.com"));
		$packet->push("answer", new Net::DNS::RR("cname1.example.com 300 A 1.2.3.4"));
	} elsif ($qname eq "cname2.example.com") {
		# Data for the "cname + other data / 2" test: same RRs in opposite order
		$packet->push("answer", new Net::DNS::RR("cname2.example.com 300 A 1.2.3.4"));
		$packet->push("answer", new Net::DNS::RR("cname2.example.com 300 CNAME cname2.example.com"));
	} elsif ($qname eq "www.example.org" || $qname eq "www.example.net" ||
		 $qname eq "badcname.example.org" ||
		 $qname eq "goodcname.example.org" ||
		 $qname eq "foo.baddname.example.org" ||
		 $qname eq "foo.gooddname.example.org") {
		# Data for address/alias filtering.
		$packet->header->aa(1);
		if ($qtype eq "A") {
			$packet->push("answer",
				      new Net::DNS::RR($qname .
						       " 300 A 192.0.2.1"));
		} elsif ($qtype eq "AAAA") {
			$packet->push("answer",
				      new Net::DNS::RR($qname .
						" 300 AAAA 2001:db8:beef::1"));
		}
	} elsif ($qname eq "badcname.example.net" ||
		 $qname eq "goodcname.example.net") {
		# Data for CNAME/DNAME filtering.  We need to make one-level
		# delegation to avoid automatic acceptance for subdomain aliases
		$packet->push("authority", new Net::DNS::RR("example.net 300 NS ns.example.net"));
		$packet->push("additional", new Net::DNS::RR("ns.example.net 300 A 10.53.0.3"));
	} elsif ($qname =~ /^nodata\.example\.net$/i) {
		$packet->header->aa(1);
	} elsif ($qname =~ /^nxdomain\.example\.net$/i) {
		$packet->header->aa(1);
		$packet->header->rcode(NXDOMAIN);
	} elsif ($qname =~ /sub\.example\.org/) {
		# Data for CNAME/DNAME filtering.  The final answers are
		# expected to be accepted regardless of the filter setting.
		$packet->push("authority", new Net::DNS::RR("sub.example.org 300 NS ns.sub.example.org"));
		$packet->push("additional", new Net::DNS::RR("ns.sub.example.org 300 A 10.53.0.3"));
	} else {
		# Data for the "bogus referrals" test
		$packet->push("authority", new Net::DNS::RR("below.www.example.com 300 NS ns.below.www.example.com"));
		$packet->push("additional", new Net::DNS::RR("ns.below.www.example.com 300 A 10.53.0.3"));
	}

	$sock->send($packet->data);

	print "RESPONSE:\n";
	$packet->print;
	print "\n";
}
