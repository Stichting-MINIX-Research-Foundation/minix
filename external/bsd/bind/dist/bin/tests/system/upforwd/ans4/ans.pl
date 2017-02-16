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

# Id: ans.pl,v 1.2 2011/08/31 06:49:10 marka Exp 

#
# This is the name server from hell.  It provides canned
# responses based on pattern matching the queries, and
# can be reprogrammed on-the-fly over a TCP connection.
#
# The server listens for control connections on port 5301.
# A control connection is a TCP stream of lines like
#
#  /pattern/
#  name ttl type rdata
#  name ttl type rdata
#  ...
#  /pattern/
#  name ttl type rdata
#  name ttl type rdata
#  ...
#
# There can be any number of patterns, each associated
# with any number of response RRs.  Each pattern is a
# Perl regular expression.
#
# Each incoming query is converted into a string of the form
# "qname qtype" (the printable query domain name, space,
# printable query type) and matched against each pattern.
#
# The first pattern matching the query is selected, and
# the RR following the pattern line are sent in the
# answer section of the response.
#
# Each new control connection causes the current set of
# patterns and responses to be cleared before adding new
# ones.
#
# The server handles UDP and TCP queries.  Zone transfer
# responses work, but must fit in a single 64 k message.
#
# Now you can add TSIG, just specify key/key data with:
#
#  /pattern <key> <key_data>/
#  name ttl type rdata
#  name ttl type rdata
#
#  Note that this data will still be sent with any request for
#  pattern, only this data will be signed. Currently, this is only
#  done for TCP.


use IO::File;
use IO::Socket;
use Data::Dumper;
use Net::DNS;
use Net::DNS::Packet;
use strict;

# Ignore SIGPIPE so we won't fail if peer closes a TCP socket early
local $SIG{PIPE} = 'IGNORE';

# Flush logged output after every line
local $| = 1;

my $server_addr = "10.53.0.4";

my $udpsock = IO::Socket::INET->new(LocalAddr => "$server_addr",
   LocalPort => 5300, Proto => "udp", Reuse => 1) or die "$!";

my $tcpsock = IO::Socket::INET->new(LocalAddr => "$server_addr",
   LocalPort => 5300, Proto => "tcp", Listen => 5, Reuse => 1) or die "$!";

print "listening on $server_addr:5300.\n";

my $pidf = new IO::File "ans.pid", "w" or die "cannot open pid file: $!";
print $pidf "$$\n" or die "cannot write pid file: $!";
$pidf->close or die "cannot close pid file: $!";;
sub rmpid { unlink "ans.pid"; exit 1; };

$SIG{INT} = \&rmpid;
$SIG{TERM} = \&rmpid;

#my @answers = ();
my @rules;
sub handleUDP {
        my ($buf) = @_;
	my $packet;

	if ($Net::DNS::VERSION > 0.68) {
		$packet = new Net::DNS::Packet(\$buf, 0);
		$@ and die $@;
	} else {
		my $err;
		($packet, $err) = new Net::DNS::Packet(\$buf, 0);
		$err and die $err;
	}

        $packet->header->qr(1);
        $packet->header->aa(1);

        my @questions = $packet->question;
        my $qname = $questions[0]->qname;
        my $qtype = $questions[0]->qtype;

        # get the existing signature if any, and clear the additional section
        my $prev_tsig;
        while (my $rr = $packet->pop("additional")) {
                if ($rr->type eq "TSIG") {
                        $prev_tsig = $rr;
                }
        }

        my $r;
        foreach $r (@rules) {
                my $pattern = $r->{pattern};
		my($dbtype, $key_name, $key_data) = split(/ /,$pattern);
		print "[handleUDP] $dbtype, $key_name, $key_data \n";
                if ("$qname $qtype" =~ /$dbtype/) {
                        my $a;
                        foreach $a (@{$r->{answer}}) {
                                $packet->push("answer", $a);
                        }
			if(defined($key_name) && defined($key_data)) {
				# Sign the packet
				print "  Signing the response with " .
                                      "$key_name/$key_data\n";
                                my $tsig = Net::DNS::RR->
                                        new("$key_name TSIG $key_data");

                                # These kluges are necessary because Net::DNS
                                # doesn't know how to sign responses.  We
                                # clear compnames so that the TSIG key and
                                # algorithm name won't be compressed, and
                                # add one to arcount because the signing
                                # function will attempt to decrement it,
                                # which is incorrect in a response. Finally
                                # we set request_mac to the previous digest.
                                $packet->{"compnames"} = {};
                                $packet->{"header"}{"arcount"} += 1;
                                if (defined($prev_tsig)) {
                                        my $rmac = pack('n H*',
                                                $prev_tsig->mac_size,
                                                $prev_tsig->mac);
                                        $tsig->{"request_mac"} =
                                                unpack("H*", $rmac);
                                }
                                
				$packet->sign_tsig($tsig);
			}
                        last;
                }
        }
        #$packet->print;

        return $packet->data;
}

# namelen:
# given a stream of data, reads a DNS-formatted name and returns its
# total length, thus making it possible to skip past it.
sub namelen {
        my ($data) = @_;
        my $len = 0;
        my $label_len = 0;
        do {
                $label_len = unpack("c", $data);
                $data = substr($data, $label_len + 1);
                $len += $label_len + 1;
        } while ($label_len != 0);
        return ($len);
}

# packetlen:
# given a stream of data, reads a DNS wire-format packet and returns
# its total length, making it possible to skip past it.
sub packetlen {
        my ($data) = @_;
        my $q;
        my $rr;

        my ($header, $offset) = Net::DNS::Header->parse(\$data);
        for (1 .. $header->qdcount) {
                ($q, $offset) = Net::DNS::Question->parse(\$data, $offset);
        }
        for (1 .. $header->ancount) {
                ($rr, $offset) = Net::DNS::RR->parse(\$data, $offset);
        }
        for (1 .. $header->nscount) {
                ($rr, $offset) = Net::DNS::RR->parse(\$data, $offset);
        }
        for (1 .. $header->arcount) {
                ($rr, $offset) = Net::DNS::RR->parse(\$data, $offset);
        }
        return $offset;
}

# sign_tcp_continuation:
# This is a hack to correct the problem that Net::DNS has no idea how
# to sign multiple-message TCP responses.  Several data that are included
# in the digest when signing a query or the first message of a response are
# omitted when signing subsequent messages in a TCP stream.
#
# Net::DNS::Packet->sign_tsig() has the ability to use a custom signing
# function (specified by calling Packet->sign_func()).  We use this
# function as the signing function for TCP continuations, and it removes
# the unwanted data from the digest before calling the default sign_hmac
# function.
sub sign_tcp_continuation {
        my ($key, $data) = @_;

        # copy out first two bytes: size of the previous MAC
        my $rmacsize = unpack("n", $data);
        $data = substr($data, 2);

        # copy out previous MAC
        my $rmac = substr($data, 0, $rmacsize);
        $data = substr($data, $rmacsize);

        # try parsing out the packet information
        my $plen = packetlen($data);
        my $pdata = substr($data, 0, $plen);
        $data = substr($data, $plen);

        # remove the keyname, ttl, class, and algorithm name
        $data = substr($data, namelen($data));
        $data = substr($data, 6);
        $data = substr($data, namelen($data));

        # preserve the TSIG data
        my $tdata = substr($data, 0, 8);

        # prepare a new digest and sign with it
        $data = pack("n", $rmacsize) . $rmac . $pdata . $tdata;
        return Net::DNS::RR::TSIG::sign_hmac($key, $data);
}

sub handleTCP {
	my ($buf) = @_;
	my $packet;

	if ($Net::DNS::VERSION > 0.68) {
		$packet = new Net::DNS::Packet(\$buf, 0);
		$@ and die $@;
	} else {
		my $err;
		($packet, $err) = new Net::DNS::Packet(\$buf, 0);
		$err and die $err;
	}
	
	$packet->header->qr(1);
	$packet->header->aa(1);
	
	my @questions = $packet->question;
	my $qname = $questions[0]->qname;
	my $qtype = $questions[0]->qtype;

        # get the existing signature if any, and clear the additional section
        my $prev_tsig;
        my $signer;
        while (my $rr = $packet->pop("additional")) {
                if ($rr->type eq "TSIG") {
                        $prev_tsig = $rr;
                }
        }

	my @results = ();
	my $count_these = 0;

	my $r;
	foreach $r (@rules) {
		my $pattern = $r->{pattern};
		my($dbtype, $key_name, $key_data) = split(/ /,$pattern);
		print "[handleTCP] $dbtype, $key_name, $key_data \n";
		if ("$qname $qtype" =~ /$dbtype/) {
			$count_these++;
			my $a;
			foreach $a (@{$r->{answer}}) {
				$packet->push("answer", $a);
			}
			if(defined($key_name) && defined($key_data)) {
				# sign the packet
				print "  Signing the data with " . 
                                      "$key_name/$key_data\n";

                                my $tsig = Net::DNS::RR->
                                        new("$key_name TSIG $key_data");

                                # These kluges are necessary because Net::DNS
                                # doesn't know how to sign responses.  We
                                # clear compnames so that the TSIG key and
                                # algorithm name won't be compressed, and
                                # add one to arcount because the signing
                                # function will attempt to decrement it,
                                # which is incorrect in a response. Finally
                                # we set request_mac to the previous digest.
                                $packet->{"compnames"} = {};
                                $packet->{"header"}{"arcount"} += 1;
                                if (defined($prev_tsig)) {
                                        my $rmac = pack('n H*',
                                                $prev_tsig->mac_size,
                                                $prev_tsig->mac);
                                        $tsig->{"request_mac"} =
                                                unpack("H*", $rmac);
                                }
                                
                                $tsig->sign_func($signer) if defined($signer);
				$packet->sign_tsig($tsig);
                                $signer = \&sign_tcp_continuation;

                                my $copy =
                                        Net::DNS::Packet->new(\($packet->data));
                                $prev_tsig = $copy->pop("additional");
			}
			#$packet->print;
			push(@results,$packet->data);
			$packet = new Net::DNS::Packet(\$buf, 0);
			$packet->header->qr(1);
			$packet->header->aa(1);
		}
	}
	print " A total of $count_these patterns matched\n";
	return \@results;
}

# Main
my $rin;
my $rout;
for (;;) {
	$rin = '';
	vec($rin, fileno($tcpsock), 1) = 1;
	vec($rin, fileno($udpsock), 1) = 1;

	select($rout = $rin, undef, undef, undef);

	if (vec($rout, fileno($udpsock), 1)) {
		printf "UDP request\n";
		my $buf;
		$udpsock->recv($buf, 512);
	} elsif (vec($rout, fileno($tcpsock), 1)) {
		my $conn = $tcpsock->accept;
		my $buf;
		for (;;) {
			my $lenbuf;
			my $n = $conn->sysread($lenbuf, 2);
			last unless $n == 2;
			my $len = unpack("n", $lenbuf);
			$n = $conn->sysread($buf, $len);
		}
		sleep(1);
	}
}
