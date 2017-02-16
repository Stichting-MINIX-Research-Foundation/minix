#!/usr/bin/perl

use warnings;
use strict;

use POSIX qw(strftime);
my $now = strftime "%Y%m%d%H%M%S", gmtime;

sub ext8601 ($) {
	my $d = shift;
	$d =~ s{(....)(..)(..)(..)(..)(..)}
	       {$1-$2-$3.$4:$5:$6+0000};
	return $d;
}

sub getkey ($$) {
	my $h = shift;
	my $k = shift;
	m{\s+(\d+)\s+(\d+)\s+(\d+)\s+[(]\s*$};
	$k->{flags}     = $1;
	$k->{protocol}  = $2;
	$k->{algorithm} = $3;
	my $data = "(";
	while (<$h>) {
		s{^\s+}{};
		s{\s+$}{};
		last if m{^[)]};
		$data .= $_;
	}
	m{ alg = (\S+); key id = (\d+)};
	$k->{alg}  = $1;
	$k->{id}   = $2;
	$k->{data} = $data;
	return $k;
}

sub fmtkey ($) {
	my $k = shift;
	return sprintf "%16s tag %s", $k->{name}, $k->{id};
}

sub printstatus ($) {
	my $a = shift;
	if ($a->{removehd} ne "19700101000000") {
		printf " untrusted and to be removed at %s\n", ext8601 $a->{removehd};
	} elsif ($a->{addhd} lt $now) {
		printf " trusted\n";
	} else {
		printf " waiting for %s\n", ext8601 $a->{addhd};
	}
}

sub digkeys ($) {
	my $name = shift;
	my $keys;
	open my $d, "-|", qw{dig +multiline DNSKEY}, $name;
	while (<$d>) {
		next unless m{^([a-z0-9.-]*)\s+\d+\s+IN\s+DNSKEY\s+};
		next unless $name eq $1;
		push @$keys, getkey $d, { name => $name };
	}
	return $keys;
}

my $anchor;
my $owner = ".";
while (<>) {
	next unless m{^([a-z0-9.-]*)\s+KEYDATA\s+(\d+)\s+(\d+)\s+(\d+)\s+};
	my $k = getkey *ARGV, {
		name     => $1,
		refresh  => $2,
		addhd    => $3,
		removehd => $4,
	};
	if ($k->{name} eq "") {
		$k->{name} = $owner;
	} else {
		$owner = $k->{name};
	}
	$k->{name} =~ s{[.]*$}{.};
	push @{$anchor->{$k->{name}}}, $k;
}

for my $name (keys %$anchor) {
	my $keys = digkeys $name;
	my $anchors = $anchor->{$name};
	for my $k (@$keys) {
		if ($k->{flags} & 1) {
			printf "%s %s", fmtkey $k, $k->{alg};
		} else {
			# ZSK - skipping
			next;
		}
		if ($k->{flags} & 512) {
			print " revoked;";
		}
		my $a;
		for my $t (@$anchors) {
			if ($t->{data} eq $k->{data} and
			    $t->{protocol} eq $k->{protocol} and
			    $t->{algorithm} eq $k->{algorithm}) {
				$t->{matched} = 1;
				$a = $t;
				last;
			}
		}
		if (not defined $a) {
			print " no trust anchor\n";
			next;
		}
		printstatus $a;
	}
	for my $a (@$anchors) {
		next if $a->{matched};
		printf "%s %s missing;", fmtkey $a, $a->{alg};
		printstatus $a;
	}
}

exit;

__END__

=head1 NAME

check5011 - summarize DNSSEC trust anchor status

=head1 SYNOPSIS

check5011 <I<managed-keys.bind>>

=head1 DESCRIPTION

The BIND managed-keys file contains DNSSEC trust anchors
that can be automatically updated according to RFC 5011. The
B<check5011> program reads this file and prints a summary of the
status of the trust anchors. It fetches the corresponding
DNSKEY records using B<dig> and compares them to the trust anchors.

Each key is printed on a line with its name, its tag, and its
algorithm, followed by a summary of its status.

=over

=item C<trusted>

The key is currently trusted.

=item C<waiting for ...>

The key is new, and B<named> is waiting for the "add hold-down" period
to pass before the key will be trusted.

=item C<untrusted and to be removed at ...>

The key was revoked and will be removed at the stated time.

=item C<no trust anchor>

The key is present in the DNS but not in the managed-keys file.

=item C<revoked>

The key has its revoked flag set. This is printed before the key's
trust anchor status which should normally be C<untrusted...> if
B<named> has observed the revocation.

=item C<missing>

There is no DNSKEY record for this trust anchor. This is printed
before the key's trust anchor status.

=back

By default the managed keys are stored in a file called
F<managed-keys.bind> in B<named>'s working directory. This location
can be changed with B<named>'s B<managed-keys-directory> option. If
you are using views the file may be named with the SHA256 hash of a
view name with a F<.mkeys> extension added.

=head1 AUTHOR

=over

=item Written by Tony Finch <fanf2@cam.ac.uk> <dot@dotat.at>

=item at the University of Cambridge Computing Service.

=item You may do anything with this. It has no warranty.

=item L<http://creativecommons.org/publicdomain/zero/1.0/>

=back

=head1 SEE ALSO

dig(1), named(8)

=cut
