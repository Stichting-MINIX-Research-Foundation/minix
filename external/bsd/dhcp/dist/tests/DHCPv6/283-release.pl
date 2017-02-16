#! /usr/bin/perl -w

# Copyright (c) 2007,2009 by Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#   Internet Systems Consortium, Inc.
#   950 Charter Street
#   Redwood City, CA 94063
#   <info@isc.org>
#   https://www.isc.org/

use strict;
use English;
use Time::HiRes qw( sleep );
use Socket;
use Socket6;
use IO::Select;

use dhcp_client;

# XXX: for debugging
use Data::Dumper;

# not-yet-standard options
my $OPT_TIME_SERVERS = 40;
my $OPT_TIME_OFFSET = 41;

# DOCSIS sub-options
my $DOCSIS_OPT_ORO = 1;
# 2 to 31 are reserved
my $DOCSIS_OPT_TFTP_SERVERS = 32;
my $DOCSIS_OPT_CONFIG_FILE_NAME = 33;
my $DOCSIS_OPT_SYSLOG_SERVERS = 34;
my $DOCSIS_OPT_TLV5 = 35;
my $DOCSIS_OPT_DEVICE_ID = 36;
my $DOCSIS_OPT_CCC = 37;
my $DOCSIS_OPT_VERS = 38;

# well-known addresses
my $All_DHCP_Relay_Agents_and_Servers = "ff02::1:2";
my $All_DHCP_Servers = "ff05::1:3";

# ports
my $client_port = 546;
my $server_port = 547;

# create a new Solicit message
my $msg = dhcp_client::msg->new($MSG_RELEASE);

# add the Client Identifier (required by DOCSIS and RFC 3315)
my $client_id = "\x00\x01\x00\x01\x0c\x00\xa1\x41\x00\x06\x5b\x50\x99\xf6";
$msg->add_option($OPT_CLIENTID, $client_id);
#$msg->add_option($OPT_CLIENTID, dhcp_client::duid(3));

# add the Server identifier (required by RFC 3315)
$msg->add_option($OPT_SERVERID, "InfiniteEntropy");

# add Elapsed Time, set to 0 on first packet (required by RFC 3315)
$msg->add_option($OPT_ELAPSED_TIME, "\x00\x00");

# add IA_NA for each interface (required by DOCSIS and RFC 3315)
# XXX: should this be a single interface only?
my $iaid = 0;
foreach my $iface (dhcp_client::iface()) {
        my $iaaddr_option = inet_pton(AF_INET6, "3ffe:aaaa:aaaa:aaaa::ffff");
        $iaaddr_option .= pack("NN", 0, 0);
        my $option_data = pack("NNN", ++$iaid, 0, 0);
        $option_data .= pack("nn", $OPT_IAADDR, length($iaaddr_option)-8);
        $option_data .= $iaaddr_option;
        $msg->add_option($OPT_IA_NA, $option_data);
}

# timeout parameters, from DOCSIS
my $IRT = $SOL_TIMEOUT;
my $MRT = $SOL_MAX_RT;
my $MRC = 4;	# DOCSIS says 4, RFC 3315 says it SHOULD be 0
my $MRD = 0;

# sleep a random amount of time between 0 and 1 second, required by RFC 3315
# XXX: this seems pretty stupid
sleep(rand($SOL_MAX_DELAY));

my $RT;
my $count = 0;
my $mrd_end_time;
if ($MRD != 0) {
	$mrd_end_time = time() + $MRD;
}
my $reply_msg;
do {
	# create our socket, and send our Solicit
	socket(SOCK, PF_INET6, SOCK_DGRAM, getprotobyname('udp')) || die;
	my $addr = inet_pton(AF_INET6, $All_DHCP_Servers);
	my $packet = $msg->packet();
	my $send_ret = send(SOCK, $packet, 0, 
			    pack_sockaddr_in6($server_port, $addr));
	if (not defined($send_ret)) {
		printf STDERR 
			"Error \%d sending DHCPv6 Solicit message;\n\%s\n",
			0+$ERRNO, $ERRNO;
		exit(1);
	} elsif ($send_ret != length($packet)) {
		print STDERR "Unable to send entire DHCPv6 Solicit message.\n";
		exit(1);
	}
	$count++;

	my $RAND = rand(0.2) - 0.1;
	if (defined $RT) {
		$RT = 2*$RT + $RAND*$RT;
		if (($RT > $MRT) && ($MRT != 0)) {
			$RT = $MRT + $RAND*$RT;
		}
	} else {
		$RT = $IRT + $RAND*$IRT;
	}

	my $rt_end_time = time() + $RT;
	if (defined($mrd_end_time) && ($mrd_end_time > $rt_end_time)) { 
		$rt_end_time = $mrd_end_time;
	}

	for (;;) {
		my $timeout = $rt_end_time - time();
		if ($timeout < 0) {
#			print STDERR "Timeout waiting for DHCPv6 Advertise ",
#				"or Reply message.\n";
			last;
		}

		my @ready = IO::Select->new(\*SOCK)->can_read($timeout);

		if (@ready) {
			my $reply;
			my $recv_ret;
	
			$recv_ret = recv(SOCK, $reply, 1500, 0);
			if (not defined $recv_ret) {
				printf STDERR 
					"Error \%d receiving DHCPv6 " . 
						"message;\n\%s\n",
					0+$ERRNO, $ERRNO;
				exit(1);
			}

			$reply_msg = dhcp_client::msg::decode($reply, 1);
			if (($reply_msg->{msg_type} == $MSG_ADVERTISE) ||
			    ($reply_msg->{msg_type} == $MSG_REPLY)) {
			    	last;
			}
		}
	}

} until ($reply_msg || 
	 (($MRC != 0) && ($count > $MRC)) ||
	 (defined($mrd_end_time) && ($mrd_end_time > time())));

unless ($reply_msg) {
	if (($MRC != 0) && ($count >= $MRC)) {
		print STDERR 
			"No reply after maximum retransmission count.\n";
	} else {
		print STDERR 
			"No reply after maximum retransmission duration.\n";
	}
}

if ($reply_msg && ($reply_msg->{msg_type} == $MSG_REPLY)) {
	print "Got DHCPv6 Reply message.\n";
	exit(0);
}

#$Data::Dumper::Useqq = 1;
#print Dumper($msg), "\n";
#print Dumper($msg->packet()), "\n";
#
#print "packet length: ", length($msg->packet()), "\n";

