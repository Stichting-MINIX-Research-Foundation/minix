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

package dhcp_client;

require Exporter;

@ISA = qw(Exporter);

# message types
$MSG_SOLICIT = 1;
$MSG_ADVERTISE = 2;
$MSG_REQUEST = 3;
$MSG_CONFIRM = 4;
$MSG_RENEW = 5;
$MSG_REBIND = 6;
$MSG_REPLY = 7;
$MSG_RELEASE = 8;
$MSG_DECLINE = 9;
$MSG_RECONFIGURE = 10;
$MSG_INFORMATION_REQUEST = 11;
$MSG_RELAY_FORW = 12;
$MSG_RELAY_REPL = 13;

# option numbers
$OPT_CLIENTID = 1;
$OPT_SERVERID = 2;
$OPT_IA_NA = 3;
$OPT_IA_TA = 4;
$OPT_IAADDR = 5;
$OPT_ORO = 6;
$OPT_PREFERENCE = 7;
$OPT_ELAPSED_TIME = 8;
$OPT_RELAY_MSG = 9;
$OPT_AUTH = 11;
$OPT_UNICAST = 12;
$OPT_STATUS_CODE = 13;
$OPT_RAPID_COMMIT = 14;
$OPT_USER_CLASS = 15;
$OPT_VENDOR_CLASS = 16;
$OPT_VENDOR_OPTS = 17;
$OPT_INTERFACE_ID = 18;
$OPT_RECONF_MSG = 19;
$OPT_RECONF_ACCEPT = 20;

# timeouts
$SOL_MAX_DELAY = 1;
$SOL_TIMEOUT = 1;
$SOL_MAX_RT = 120;
$REQ_TIMEOUT = 1;
$REQ_MAX_RT = 30;
$REQ_MAX_RC = 10;
$CNF_MAX_DELAY = 1;
$CNF_MAX_RT = 4;
$CNF_MAX_RD = 10;
$REN_TIMEOUT = 10;
$REN_MAX_RT = 600;
$REB_TIMEOUT = 10;
$REB_MAX_RT = 600;
$INF_MAX_DELAY = 1;
$INF_TIMEOUT = 1;
$INF_MAX_RT = 120;
$REL_TIMEOUT = 1;
$REL_MAX_RC = 5;
$DEC_TIMEOUT = 1;
$DEC_MAX_RC = 5;
$REC_TIMEOUT = 2;
$REC_MAX_RC = 8;
$HOP_COUNT_LIMIT = 32;

@EXPORT = qw( $MSG_SOLICIT $MSG_ADVERTISE $MSG_REQUEST $MSG_CONFIRM
	      $MSG_RENEW $MSG_REBIND $MSG_REPLY $MSG_RELEASE $MSG_DECLINE
	      $MSG_RECONFIGURE $MSG_INFORMATION_REQUEST $MSG_RELAY_FORW
	      $MSG_RELAY_REPL 
	      $OPT_CLIENTID $OPT_SERVERID $OPT_IA_NA $OPT_IA_TA $OPT_IAADDR
	      $OPT_ORO $OPT_PREFERENCE $OPT_ELAPSED_TIME $OPT_RELAY_MSG
	      $OPT_AUTH $OPT_UNICAST $OPT_STATUS_CODE $OPT_RAPID_COMMIT
	      $OPT_USER_CLASS $OPT_VENDOR_CLASS $OPT_VENDOR_OPTS 
	      $OPT_INTERFACE_ID $OPT_RECONF_MSG $OPT_RECONF_ACCEPT 
	      $SOL_MAX_DELAY $SOL_TIMEOUT $SOL_MAX_RT $REQ_TIMEOUT
	      $REQ_MAX_RT $REQ_MAX_RC $CNF_MAX_DELAY $CNF_MAX_RT
	      $CNF_MAX_RD $REN_TIMEOUT $REN_MAX_RT $REB_TIMEOUT $REB_MAX_RT
	      $INF_MAX_DELAY $INF_TIMEOUT $INF_MAX_RT $REL_TIMEOUT
	      $REL_MAX_RC $DEC_TIMEOUT $DEC_MAX_RC $REC_TIMEOUT $REC_MAX_RC
	      $HOP_COUNT_LIMIT );

my %msg_type_num = (
	MSG_SOLICIT => 1,
	MSG_ADVERTISE => 2,
	MSG_REQUEST => 3,
	MSG_CONFIRM => 4,
	MSG_RENEW => 5,
	MSG_REBIND => 6,
	MSG_REPLY => 7,
	MSG_RELEASE => 8,
	MSG_DECLINE => 9,
	MSG_RECONFIGURE => 10,
	MSG_INFORMATION_REQUEST => 11,
	MSG_RELAY_FORW => 12,
	MSG_RELAY_REPL => 13,
);
my %msg_num_type = reverse(%msg_type_num);

my %opt_type_num = (
	OPT_CLIENTID => 1,
	OPT_SERVERID => 2,
	OPT_IA_NA => 3,
	OPT_IA_TA => 4,
	OPT_IAADDR => 5,
	OPT_ORO => 6,
	OPT_PREFERENCE => 7,
	OPT_ELAPSED_TIME => 8,
	OPT_RELAY_MSG => 9,
	OPT_AUTH => 11,
	OPT_UNICAST => 12,
	OPT_STATUS_CODE => 13,
	OPT_RAPID_COMMIT => 14,
	OPT_USER_CLASS => 15,
	OPT_VENDOR_CLASS => 16,
	OPT_VENDOR_OPTS => 17,
	OPT_INTERFACE_ID => 18,
	OPT_RECONF_MSG => 19,
	OPT_RECONF_ACCEPT => 20,
);
my %opt_num_type = reverse(%opt_type_num);

my %status_code_num = (
	Success => 0,
	UnspecFail => 1,
	NoAddrsAvail => 2,
	NoBinding => 3,
	NotOnLink => 4,
	UseMulticast => 5,
);
my %status_num_code = reverse(%status_code_num);

my %docsis_type_num = (
	CL_OPTION_ORO => 1,
	CL_OPTION_TFTP_SERVERS => 32,
	CL_OPTION_CONFIG_FILE_NAME => 33,
	CL_OPTION_SYSLOG_SERVERS => 34,
	CL_OPTION_TLV5 => 35,
	CL_OPTION_DEVICE_ID => 36,
	CL_OPTION_CCC => 37,
	CL_OPTION_DOCSIS_VERS => 38,
);
my %docsis_num_type = reverse(%docsis_type_num);
	
use strict;
use English;
use POSIX;

# XXX: very Solaris-specific
sub iface {
	my @ifaces;
	foreach my $fname (glob("/etc/hostname.*")) {
		$fname =~ s[^/etc/hostname.][];
		push(@ifaces, $fname);
	}
	return wantarray() ? @ifaces : $ifaces[0];
}

# XXX: very Solaris-specific
sub mac_addr {
	my @ip_addrs;
	foreach my $iface (iface()) {
		if (`ifconfig $iface 2>/dev/null` =~ /\sinet (\S+)\s/) {
			push(@ip_addrs, $1);
		}
	}
	my @mac_addrs;
	foreach my $line (split(/\n/, `arp -an 2>/dev/null`)) { 
		my @parts = split(/\s+/, $line);
		my $ip = $parts[1];
		my $mac = $parts[-1];
		if (grep { $ip eq $_ }  @ip_addrs) {
			$mac =~ s/://g;
			push(@mac_addrs, $mac);
		}
	}
	return wantarray() ? @mac_addrs : $mac_addrs[0];
}

sub mac_addr_binary {
	my @mac_addr = split(//, mac_addr());
	my $mac_addr = join("", map { chr(hex($_)) } @mac_addr);
	return $mac_addr;
}

# DHCPv6 times start 2000-01-01 00:00:00
my $dhcp_time_base = 946684800;
#{
#	local $ENV{TZ} = "UTC";
#	POSIX::tzset();
#	$dhcp_time_base = POSIX::mktime(0, 0, 0, 1, 0, 100);
#}

sub dhcpv6_time {
	return time() - $dhcp_time_base;
}

sub duid {
	my ($type) = @_;

	$type = 1 unless (defined $type);

	if (($type == 1) || ($type == 3)) {
		my $mac_addr = mac_addr_binary();
		if ($type == 1) { 
			my $time = pack("N", dhcpv6_time());
			return "\x00\x01\x00\x01${time}${mac_addr}";
		} else {
			return "\x00\x03\x00\x01${mac_addr}";
		}
	} else {
		die "Unknown DUID type $type requested";
	}
}

package dhcp_client::msg;

use Socket;
use Socket6;

sub new {
	my ($pkg, $msg_type, $trans_id) = @_;

	my $this = {};
	bless $this;

	$this->{msg_type} = $msg_type+0;
	if (defined $trans_id) {
		$this->{trans_id} = $trans_id;
	} else {
		$this->{trans_id} = chr(rand(256)) . 
			chr(rand(256)) . chr(rand(256));
	}
	$this->{options} = [ ];

	return $this;
}


sub add_option {
	my ($this, $num, $data) = @_;

	push(@{$this->{options}}, [ $num, $data ]);
}

sub get_option {
	my ($this, $num) = @_;
	my @options;
	foreach my $option (@{$this->{options}}) {
		if ($option->[0] == $num) {
			push(@options, $option->[1]);
		}
	}
	return wantarray() ? @options : $options[0];
}

sub packed_options {
	my ($this) = @_;

	my $options = "";
	foreach my $option (@{$this->{options}}) {
		$options .= pack("nn", $option->[0], length($option->[1]));
		$options .= $option->[1];
	}
	return $options;
}

sub packet {
	my ($this) = @_;

	my $packet = "";
	$packet .= chr($this->{msg_type});
	$packet .= $this->{trans_id};
	$packet .= $this->packed_options();
	return $packet;
}

sub unpack_options {
	my ($options) = @_;

	my @parsed_options;
	my $p = 0;
	while ($p < length($options)) {
		my ($id, $len) = unpack("nn", substr($options, $p, 4));
		push(@parsed_options, [ $id,  substr($options, $p + 4, $len) ]);
		$p += 4 + $len;
	}
	return @parsed_options;
}

sub print_docsis_option {
	my ($num, $data, $indent) = @_;

	print "${indent}DOCSIS Option $num";
	if ($docsis_num_type{$num}) {
		print " ($docsis_num_type{$num})";
	}
	print ", length ", length($data), "\n";

	return unless ($docsis_num_type{$num});

	if ($docsis_num_type{$num} eq "CL_OPTION_ORO") {
		my $num_oro = length($data) / 2;
		for (my $i=0; $i<$num_oro; $i++) {
			my $oro_num = unpack("n", substr($data, $i*2, 2));
			print "${indent}  $oro_num";
			if ($docsis_num_type{$oro_num}) {
				print " ($docsis_num_type{$oro_num})";
			}
			print "\n";
		}
	} elsif ($docsis_num_type{$num} eq "CL_OPTION_TFTP_SERVERS") {
		my $num_servers = length($data) / 16;
		for (my $i=0; $i<$num_servers; $i++) {
			my $srv = inet_ntop(AF_INET6, substr($data, $i*16, 16));
			print "$indent  TFTP server ", ($i+1), ": "; 
			print uc($srv), "\n";
		}
	} elsif ($docsis_num_type{$num} eq "CL_OPTION_CONFIG_FILE_NAME") {
		print "$indent  Config file name: \"$data\"\n"
	} elsif ($docsis_num_type{$num} eq "CL_OPTION_SYSLOG_SERVERS") {
		my $num_servers = length($data) / 16;
		for (my $i=0; $i<$num_servers; $i++) {
			my $srv = inet_ntop(AF_INET6, substr($data, $i*16, 16));
			print "$indent  syslog server ", ($i+1), ": "; 
			print uc($srv), "\n";
		}
	}
}

sub print_option {
	my ($num, $data, $indent) = @_;

	print "${indent}Option $num";
	if ($opt_num_type{$num}) {
		print " ($opt_num_type{$num})";
	}
	print ", length ", length($data), "\n";
	if ($num == $dhcp_client::OPT_ORO) {
		my $num_oro = length($data) / 2;
		for (my $i=0; $i<$num_oro; $i++) {
			my $oro_num = unpack("n", substr($data, $i*2, 2));
			print "${indent}  $oro_num";
			if ($opt_num_type{$oro_num}) {
				print " ($opt_num_type{$oro_num})";
			}
			print "\n";
		}
	} elsif (($num == $dhcp_client::OPT_CLIENTID) || 
		 ($num == $dhcp_client::OPT_SERVERID)) {
		print $indent, "  ";
		if (length($data) > 0) {
			printf '%02X', ord(substr($data, 0, 1));
			for (my $i=1; $i<length($data); $i++) {
				printf ':%02X', ord(substr($data, $i, 1));
			}
		}
		print "\n";
	} elsif ($num == $dhcp_client::OPT_IA_NA) {
		printf "${indent}  IAID: 0x\%08X\n", 
			unpack("N", substr($data, 0, 4));
		printf "${indent}  T1: \%d\n", unpack("N", substr($data, 4, 4));
		printf "${indent}  T2: \%d\n", unpack("N", substr($data, 8, 4));
		if (length($data) > 12) {
			printf "${indent}  IA_NA encapsulated options:\n";
			foreach my $option (unpack_options(substr($data, 12))) {
				print_option(@{$option}, $indent . "    ");
			}
		}
	} elsif ($num == $dhcp_client::OPT_IAADDR) {
		printf "${indent}  IPv6 address: \%s\n", 
			uc(inet_ntop(AF_INET6, substr($data, 0, 16)));
		printf "${indent}  Preferred lifetime: \%d\n",
			unpack("N", substr($data, 16, 4));
		printf "${indent}  Valid lifetime: \%d\n",
			unpack("N", substr($data, 20, 4));
		if (length($data) > 24) {
			printf "${indent}  IAADDR encapsulated options:\n";
			foreach my $option (unpack_options(substr($data, 24))) {
				print_option(@{$option}, $indent . "    ");
			}
		}
	} elsif ($num == $dhcp_client::OPT_VENDOR_OPTS) {
		my $enterprise_number = unpack("N", substr($data, 0, 4));
		print "${indent}  Enterprise number: $enterprise_number\n";

		# DOCSIS
		if ($enterprise_number == 4491) {
			foreach my $option (unpack_options(substr($data, 4))) {
				print_docsis_option(@{$option}, $indent . "  ");
			}
		}
	} elsif ($num == $dhcp_client::OPT_STATUS_CODE) {
		my $code = ord(substr($data, 0, 1));
		my $msg = substr($data, 1);
		print "${indent}  Code: $code";
		if ($status_num_code{$code}) {
			print " ($status_num_code{$code})";
		}
		print "\n";
		print "${indent}  Message: \"$msg\"\n";
	} 
}

# XXX: we aren't careful about packet boundaries and values... 
#       DO NOT RUN ON PRODUCTION SYSTEMS!!!
sub decode {
	my ($packet, $print) = @_;

	my $msg_type = ord(substr($packet, 0, 1));
	my $trans_id = substr($packet, 1, 3);
	my $msg = dhcp_client::msg->new($msg_type, $trans_id);

	if ($print) {
		print "DHCPv6 packet\n";
		print "  Message type:   $msg_num_type{$msg_type}\n";
		printf "  Transaction id: 0x\%02X\%02X\%02X\n",
			ord(substr($trans_id, 0, 1)),
			ord(substr($trans_id, 1, 1)),
			ord(substr($trans_id, 2, 1));
		print "  Options:\n";
	}

	foreach my $option (unpack_options(substr($packet, 4))) {
		print_option(@{$option}, "    ") if ($print);
		$msg->add_option(@{$option});
	}

	return $msg;
}

