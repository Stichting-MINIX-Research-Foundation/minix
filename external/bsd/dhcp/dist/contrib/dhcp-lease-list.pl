#!/usr/bin/perl
#
# Shows current leases.
#
# THIS SCRIPT IS PUBLIC DOMAIN, NO RIGHTS RESERVED!
#
# I've removed the email addresses of Christian and vom to avoid
# putting them on spam lists.  If either of you would like to have
# your email in here please send mail to the DHCP bugs list at ISC.
#
# 2008-07-13, Christian Hammers
#
# 2009-06-?? - added loading progress counter, pulls hostname, adjusted formatting
#        vom
#
# 2013-04-22 - added option to choose lease file, made manufacture information
#              optional, sar
use strict;
use warnings;
use POSIX qw(strftime);

my $LEASES = '/var/db/dhcpd.leases';
my @all_leases;
my @leases;

my @OUIS = ('/usr/share/misc/oui.txt', '/usr/local/etc/oui.txt');
my $OUI_URL = 'http://standards.ieee.org/regauth/oui/oui.txt';
my $oui;

my %data;

my $opt_format = 'human';
my $opt_keep = 'active';

our $total_leases = 0;

## Return manufactorer name for specified MAC address (aa:bb:cc:dd:ee:ff).
sub get_manufactorer_for_mac($) {
    my $manu = "-NA-";

    if (defined $oui) {
	$manu = join('-', ($_[0] =~ /^(..):(..):(..):/));
	$manu = `grep -i '$manu' $oui | cut -f3`;
	chomp($manu);
    }

    return $manu;
}

## Read oui.txt or print warning.
sub check_oui_file() {

    for my $oui_cand (@OUIS) {
        if ( -r $oui_cand) {
        $oui = $oui_cand;
        last;
        }
    }

    if (not defined $oui) {
	print(STDERR "To get manufacturer names please download $OUI_URL ");
	print(STDERR "to /usr/local/etc/oui.txt\n");
    }
}

## Read current leases file into array.
sub read_dhcpd_leases() {

    open(F, $LEASES) or die("Cannot open $LEASES: $!");
    my $content = join('', <F>);
    close(F);
    @all_leases = split(/lease/, $content);

    foreach my $lease (@all_leases) {
    if ($lease =~ /^\s+([\.\d]+)\s+{.*starts \d+ ([\/\d\ \:]+);.*ends \d+ ([\/\d\ \:]+);.*ethernet ([a-f0-9:]+);/s) {
       ++$total_leases;
       }
    }
}

## Add manufactor name and sort out obsolet assignements.
sub process_leases() {
    my $gm_now = strftime("%Y/%m/%d %H:%M:%S", gmtime());
    my %tmp_leases; # for sorting and filtering

    my $counter = 1;

    # parse entries
    foreach my $lease (@all_leases) {
	# skip invalid lines
	next if not ($lease =~ /^\s+([\.\d]+)\s+{.*starts \d+ ([\/\d\ \:]+);.*ends \d+ ([\/\d\ \:]+);.*ethernet ([a-f0-9:]+);(.*client-hostname \"(\S+)\";)*/s);
	# skip outdated lines
	next if ($opt_keep eq 'active'  and  $3 lt $gm_now);

	my $percent = (($counter / $total_leases)*100);
	printf "Processing: %2d%% complete\r", $percent;
	++$counter;

	 my $hostname = "-NA-";
	 if ($6) {
	     $hostname = $6;
	 }

	my $mac = $4;
	my $date_end = $3;
	my %entry = (
	    'ip' => $1,
	    'date_begin' => $2,
	    'date_end' => $date_end,
	    'mac' => $mac,
	    'hostname' => $hostname,
	    'manu' => get_manufactorer_for_mac($mac),
	    );

	$entry{'date_begin'} =~ s#\/#-#g; # long live ISO 8601
	$entry{'date_end'}   =~ s#\/#-#g;

	if ($opt_keep eq 'all') {
	    push(@leases, \%entry);
	} elsif (not defined $tmp_leases{$mac}  or  $tmp_leases{$mac}{'date_end'} gt $date_end) {
	    $tmp_leases{$mac} = \%entry;
	}
    }

    # In case we used the hash to filtered
    if (%tmp_leases) {
	foreach (sort keys %tmp_leases) {
	    my $h = $tmp_leases{$_};
	    push(@leases, $h);
	}
    }

    # print "\n";

}

# Output all valid leases.
sub output_leases() {
    if ($opt_format eq 'human') {
	printf "%-19s%-16s%-15s%-20s%-20s\n","MAC","IP","hostname","valid until","manufacturer";
	print("===============================================================================================\n");
    }
    foreach (@leases) {
       if ($opt_format eq 'human') {
	   printf("%-19s%-16s%-15s%-20s%-20s\n",
		  $_->{'mac'},       # MAC
		  $_->{'ip'},        # IP address
		  $_->{'hostname'},  # hostname
		  $_->{'date_end'},  # Date
		  $_->{'manu'});     # manufactor name
       } else {
	   printf("MAC %s IP %s HOSTNAME %s BEGIN %s END %s MANUFACTURER %s\n",
		  $_->{'mac'},
		  $_->{'ip'},
		  $_->{'hostname'},
		  $_->{'date_begin'},
		  $_->{'date_end'},
		  $_->{'manu'});
       }
    }
}

# Commandline Processing.
sub cli_processing() {
    while (my $arg = shift(@ARGV)) {
	if ($arg eq '--help') {
	    print(
		"Prints active DHCP leases.\n\n".
		"Usage: $0 [options]\n".
		" --help      shows this help\n".
		" --parsable  machine readable output with full dates\n".
		" --last      prints the last (even if end<now) entry for every MAC\n".
		" --all       prints all entries i.e. more than one per MAC\n".
		" --lease     uses the next argument as the name of the lease file\n".
		"             the default is /var/db/dhcpd.leases\n".
		"\n");
	    exit(0);
	} elsif ($arg eq '--parsable') {
	    $opt_format = 'parsable';
	} elsif ($arg eq '--last') {
	    $opt_keep = 'last';
	} elsif ($arg eq '--all') {
	    $opt_keep = 'all';
	} elsif ($arg eq '--lease') {
	    $LEASES = shift(@ARGV);
	} else {
	    die("Unknown option $arg");
	}
    }
}

#
# main()
#
cli_processing();
check_oui_file();
read_dhcpd_leases();
process_leases();
output_leases();
