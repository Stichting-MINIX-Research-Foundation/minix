#! /usr/pkg/bin/perl -w

# netpgp bindings for perl

use lib "/usr/src/crypto/external/bsd/netpgp/dist/bindings/perl";

use netpgpperl;

# initializations
$n = netpgpperlc::new_netpgp_t();
netpgpperlc::netpgp_setvar($n, "homedir", $ENV{'HOME'}."/.gnupg");
netpgpperlc::netpgp_setvar($n, "hash", "SHA256");
netpgpperlc::netpgp_init($n);

# get the default userid
$userid = netpgpperlc::netpgp_getvar($n, "userid");

foreach $i (0 .. $#ARGV) {
	# set up file names
	my $in = $ARGV[$i];
	my $out = $in . ".gpg";

	# sign the file, output is in $out
	netpgpperlc::netpgp_sign_file($n, $userid, $in, $out, 0, 0, 0);

	# verify the signed file $out
	netpgpperlc::netpgp_verify_file($n, $out, "/dev/null", 0);
}
