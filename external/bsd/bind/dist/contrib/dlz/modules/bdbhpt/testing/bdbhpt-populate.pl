#!/usr/bin/perl -w
use strict;
use BerkeleyDB;
use Getopt::Long;

my $opt = {};
if (!GetOptions($opt, qw/bdb|b:s input|i:s zones|z:s help|h/)) {
    usage('GetOptions processing failed.');
    exit 1;
}

if ($opt->{help}) {
    usage();
    exit 0;
}

my $db_file = $opt->{bdb};
if (!defined $db_file || $db_file eq '') {
    usage('Please specify an output BerkeleyDB filename.');
    exit 1;
}

my $input_file = $opt->{input};
if (!defined $input_file || $input_file eq '') {
    usage('Please specify an input records file.');
    exit 1;
}

my $zone_list = $opt->{zones};
if (!defined $zone_list || $zone_list eq '') {
    usage('Please specify a space seperated list of zones');
    exit 1;
}

my $records = [];
my $unique_names = [];
populate_records(records=>$records, input_file=>$input_file, unique_names=>$unique_names);

my $flags =  DB_CREATE;

my $dns_data = new BerkeleyDB::Hash
    -Filename  => $db_file,
    -Flags     => $flags,
    -Property  => DB_DUP | DB_DUPSORT,
    -Subname   => "dns_data"
    ||    die "Cannot create dns_data: $BerkeleyDB::Error";

my $replId = 0;
my @zones = split(/\s+/, $zone_list);
foreach my $zone (@zones) {
    foreach my $r (@$records) {
        my $name = $r->{name};
        my $ttl = $r->{ttl};
        my $type = $r->{type};
        my $data = $r->{data};
        
        $data =~ s/\%zone\%/$zone/g;
        $data =~ s/\%driver\%/bdbhpt-dynamic/g;
        
        my $row_name  = "$zone $name";
        my $row_value = "$replId $name $ttl $type $data";
        if ($dns_data->db_put($row_name, $row_value) != 0) {
            die "Cannot add record '$row_name' -> '$row_value' to dns_data: $BerkeleyDB::Error";
        }
        $replId++;
    }
}

$dns_data->db_close();

my $dns_xfr = new BerkeleyDB::Hash
    -Filename  => $db_file,
    -Flags     => $flags,
    -Property  => DB_DUP | DB_DUPSORT,
    -Subname   => "dns_xfr"
    or die "Cannot create dns_xfr: $BerkeleyDB::Error";

foreach my $zone (@zones) {
    foreach my $name (@$unique_names) {
        if ($dns_xfr->db_put($zone, $name) != 0) {
            die "Cannot add record '$zone' -> '$name' to dns_xfr: $BerkeleyDB::Error";
        }
    }
}

$dns_xfr->db_close();

my $dns_client = new BerkeleyDB::Hash
    -Filename  => $db_file,
    -Flags     => $flags,
    -Property  => DB_DUP | DB_DUPSORT,
    -Subname   => "dns_client"
    or die "Cannot create dns_client: $BerkeleyDB::Error";

foreach my $zone (@zones) {
    my $ip = '127.0.0.1';
    if ($dns_client->db_put($zone, $ip) != 0) {
        die "Cannot add record '$zone' -> '$ip' to dns_client: $BerkeleyDB::Error";
    }
}

$dns_client->db_close();

my $dns_zone = new BerkeleyDB::Btree
    -Filename  => $db_file,
    -Flags     => $flags,
    -Property  => 0,
    -Subname   => "dns_zone"
    or die "Cannot create dns_zone: $BerkeleyDB::Error";

foreach my $zone (@zones) {
    my $reversed_zone = reverse($zone);
    if ($dns_zone->db_put($reversed_zone, "1") != 0) {
        die "Cannot add record '$reversed_zone' -> '1' to dns_zone: $BerkeleyDB::Error";
    }
};

$dns_zone->db_close();

exit 0;

sub usage {
    my ($message) = @_;
    if (defined $message && $message ne '') {
        print STDERR $message . "\n\n";
    }

    print STDERR "usage: $0 --bdb=<bdb-file> --input=<input-file> --zones=<zone-list>\n\n";
    print STDERR "\tbdb-file: The output BerkeleyDB file you wish to create and use with bdbhpt-dynamic\n\n";
    print STDERR "\tinput-file: The input text-file containing records to populate within your zones\n\n";
    print STDERR "\tzone-list: The space-seperated list of zones you wish to create\n\n";
}

sub populate_records {
    my (%args) = @_;
    my $records = $args{records};
    my $input_file = $args{input_file};
    my $unique_names = $args{unique_names};

    my %unique;

    open(RECORDS, $input_file) || die "unable to open $input_file: $!";
    while (<RECORDS>) {
        chomp;
        s/\#.*$//;
        s/^\s+//;
        if ($_ eq '') {
            next;
        }
        my ($name, $ttl, $type, $data) = split(/\s+/, $_, 4);
        my $record = { name=>$name, ttl=>$ttl, type=>$type, data=>$data };
        if (validate_record($record)) {
            push @$records, $record;
            $unique{$name} = 1;
        }
    }
    close(RECORDS);

    foreach my $name (sort keys %unique) {
        push @$unique_names, $name;
    }
}

# This could probably do more in-depth tests, but these tests are better than nothing!
sub validate_record {
    my ($r) = @_;

    # http://en.wikipedia.org/wiki/List_of_DNS_record_types
    my @TYPES = qw/A AAAA AFSDB APL CERT CNAME DHCID DLV DNAME DNSKEY DS HIP IPSECKEY KEY KX LOC MX NAPTR NS NSEC NSEC3 NSEC3PARAM PTR RRSIG RP SIG SOA SPF SRV SSHFP TA TKEY TLSA TSIG TXT/;
    my $VALID_TYPE = {};
    foreach my $t (@TYPES) {
        $VALID_TYPE->{$t} = 1;
    }
    
    if (!defined $r->{name} || $r->{name} eq '') {
        die "Record name must be set";
    }

    if (!defined $r->{ttl} || $r->{ttl} eq '') {
        die "Record TTL must be set";
    }

    if ($r->{ttl} =~ /\D/ || $r->{ttl} < 0) {
        die "Record TTL must be an integer 0 or greater";
    }

    if (!defined $r->{type} || $r->{type} eq '') {
        die "Record type must be set";
    }

    if (!$VALID_TYPE->{$r->{type}}) {
        die "Unsupported record type: $r->{type}";
    }

    # Lets do some data validation for the records which will cause bind to crash if they're wrong
    if ($r->{type} eq 'SOA') {
        my $soa_error = "SOA records must take the form: 'server email refresh retry expire negative_cache_ttl'";
        my ($server, $email, $version, $refresh, $retry, $expire, $negative_cache_ttl) = split(/\s+/, $r->{data});
        if (!defined $server || $server eq '') {
            die "$soa_error, missing server";
        }
        if (!defined $email || $email eq '') {
            die "$soa_error, missing email";
        }
        if (!defined $refresh || $refresh eq '') {
            die "$soa_error, missing refresh";
        }
        if ($refresh =~ /\D/ || $refresh <= 0) {
            die "$soa_error, refresh must be an integer greater than 0";
        }
        if (!defined $retry || $retry eq '') {
            die "$soa_error, missing retry";
        }
        if ($retry =~ /\D/ || $retry <= 0) {
            die "$soa_error, retry must be an integer greater than 0";
        }
        if (!defined $expire || $expire eq '') {
            die "$soa_error, missing expire";
        }
        if ($expire =~ /\D/ || $expire <= 0) {
            die "$soa_error, expire must be an integer greater than 0";
        }
        if (!defined $negative_cache_ttl || $negative_cache_ttl eq '') {
            die "$soa_error, missing negative cache ttl";
        }
        if ($negative_cache_ttl =~ /\D/ || $negative_cache_ttl <= 0) {
            die "$soa_error, negative cache ttl must be an integer greater than 0";
        }
    }

    return 1;
}
