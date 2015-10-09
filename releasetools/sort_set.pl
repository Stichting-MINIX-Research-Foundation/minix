#!/usr/bin/perl -T -t -w -W

# Sort each line of the input, ignoring any line beginning with a #.
# Also format the output so that it is properly aligned, with exceptions
# for values which are too long for their respective field.

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
	# Ignore any line starting with a '#'
	if ($_ =~ m/#.*/) {
		next;
	}

	# Entry with a condition field, one or more whitespace characters
	# separate each column. Example:
	# ./etc/X11	minix-base	xorg
	if ($_ =~ m/(\S+)\s+(\S+)\s+(\S+)/) {
		my ($f, $s, $c) = ($1, $2, $3);
		$sets{"$f"} = $s;
		$conditions{"$f"} = $c;
	}

	# Entry without a condition field. Example:
	# ./bin		minix-base
	if ($_ =~ m/(\S+)\s+(\S+)/) {
		my ($f, $s) = ($1, $2);
		$sets{"$f"} = $s;
	}
}

# Sort by file/directory name.
foreach $file (sort keys %sets) {
	$set = $sets{$file};

	if (length($file) < 56) {
		printf("%-55s ", $file);
	} else {
		printf("%s ", $file);
	}

	$last = $set;
	if (exists($conditions{$file})) {
		# A condition is present, so make sure it is printed with
		# the required alignment, by adding the necessary padding
		# after the set column. Otherwise do not add trailing
		# spaces.
		$last = $conditions{$file};
		if (length($set) < 16) {
			printf("%-15s ", $set);
		} else {
			printf("%s ", $set);
		}
	}

	printf("%s\n", $last);
}
