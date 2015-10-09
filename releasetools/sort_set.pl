#!/usr/bin/perl -T -t -w -W

print <<HEADER;
#
# Sorted using sort_set.pl in releasetools.
# to add an entry simply add it at the end of the
# file and run
# ../../../../releasetools/sort_set.pl < mi > out
# mv out mi
#
HEADER

while(<STDIN>) {
	if ($_ =~ m/#.*/) {
		next;
	}

	if ($_ =~ m/(\S+)\s+(\S+)\s+(\S+)/) {
		my ($f, $s, $c) = ($1, $2, $3);
		$sets{"$f"} = $s;
		$conditions{"$f"} = $c;
	}

	if ($_ =~ m/(\S+)\s+(\S+)/) {
		my ($f, $s) = ($1, $2);
		$sets{"$f"} = $s;
	}
}

foreach $file (sort keys %sets) {
	$set = $sets{$file};

	if (length($file) < 56) {
		printf("%-55s ", $file);
	} else {
		printf("%s ", $file);
	}

	$last = $set;
	if (exists($conditions{$file})) {
		$last = $conditions{$file};
		if (length($set) < 16) {
			printf("%-15s ", $set);
		} else {
			printf("%s ", $set);
		}
	}

	printf("%s\n", $last);
}
