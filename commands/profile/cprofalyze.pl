#!/usr/pkg/bin/perl
#
# cprofalyze.pl 
#
# Analyzes the output files created by the profile command for
# Call Profiling.
#
# Changes:
#   14 Aug, 2006  Created (Rogier Meurs)
#

$UNSIGNED_MAX_DIV_K = 2**32/1000;


if ($#ARGV == 0 || process_args(@ARGV)) { 
	print "Usage:\n";
	print "  cprofalyze.pl <clock> [-f] [-aoct] [-i] [-n number] file ...\n\n";
	print "      clock  CPU clock of source machine in MHz (mandatory)\n";
	print "         -f  print totals per function (original order lost)\n";
	print "         -a  sort alphabetically (default)\n";
	print "         -o  no sort (original order)\n";
	print "         -c  sort by number of calls\n";
	print "         -t  sort by time spent\n";
	print "         -n  print maximum of number lines per process\n";
	print "         -i  when -[ao] used: print full paths\n";
	exit 1;
}


sub process_args {
  $_ = shift;

  return 1 unless /^(\d+)$/;
  return 1 if $1 == 0;
  $MHz = $1;

  $sort_method = "A";
  while (@_[0] =~ /^-/) {
	$_ = shift;
	SWITCH: {
		if (/^-a$/) { $sort_method = "A"; last SWITCH; }
		if (/^-o$/) { $sort_method = "O"; last SWITCH; }
		if (/^-c$/) { $sort_method = "C"; last SWITCH; }
		if (/^-t$/) { $sort_method = "T"; last SWITCH; }
		if (/^-i$/) { $print_full_paths = 1; last SWITCH; }
		if (/^-f$/) { $print_totals = 1; last SWITCH; }
		if (/^-n$/) {
			$_ = shift;
			return 1 unless /^(\d+)$/;
			return 1 unless $1 > 0;
			$show_paths = $1;
			last SWITCH; 
		}
		return 1;
  	}
  }

  $print_full_paths == 1 && ($sort_method eq "T" || $sort_method eq "C") &&
  { $print_full_paths = 0 };

  @files = @_;
  return 0;
}


print <<EOF;
Notes:
- Calls attributed to a path are calls done on that call level.
    For instance: a() is called once and calls b() twice. Call path "a" is
    attributed 1 call, call path "a b" is attributed 2 calls.
- Time spent blocking is included.
- Time attributed to a path is time spent on that call level.
    For instance: a() spends 10 cycles in its own body and calls b() which
    spends 5 cycles in its body. Call path "a" is attributed 10 cycles,
    call path "a b" is attributed 5 cycles.
- Time is attributed when a function exits. Functions calls that have not
  returned yet are therefore not measured. This is most notable in main
  functions that are printed as having zero cycles.
- When "profile reset" was run, the actual resetting in a process happens
  when a function is entered. In some processes (for example, blocking
  ones) this may not happen immediately, or at all.

EOF

print "Clockspeed entered: $MHz MHz. ";
SWITCH: {
	if ($sort_method eq "A")
		{ print "Sorting alphabetically. "; last SWITCH; }
	if ($sort_method eq "C")
		{ print "Sorting by calls. "; last SWITCH; }
	if ($sort_method eq "T")
		{ print "Sorting by time spent. "; last SWITCH; }
	print "No sorting applied. ";
}
print "\n";
$print_totals and print "Printing totals per function. ";
$show_paths == 0 ? print "Printing all call paths.\n" :
	print "Printing max. $show_paths lines per process.\n";

foreach $file (@files) {
  $file_res = read_file($file);
  next if $file_res == 0;
  print_file($print_totals ? make_totals($file_res) : $file_res);
}

exit 0;


sub read_file
{
  $file = shift;
  my %file_res = ();
  my @exe;
  my $exe_name, $slots_used, $buf, $lo, $hi, $cycles_div_k, $ms;

  unless (open(FILE, $file)) {
	print "\nERROR: Unable to open $file: $!\n";
	return 0;
  }

  $file =~ s/^.*\///;	# basename

  # First line: check file type.
  $_ = <FILE>; chomp;
  if (!/^call$/) {
	if (/^stat$/) {
		print "Statistical Profiling output file: ";
		print "Use sprofalyze.pl instead.\n";
	} else {
		print "Not a profiling output file.\n";
	}
	return 0;
  }

  # Second line: header with call path string size.
  $_ = <FILE>; chomp;
  ($CPATH_MAX_LEN, $PROCNAME_LEN) = split(/ /);
  $SLOT_SIZE		= $CPATH_MAX_LEN + 16;
  $EXE_HEADER_SIZE	= $PROCNAME_LEN + 4;

  # Read in the data for all the processes and put it in a hash of lists.
  # A list for each process, which contains lists itself for each call
  # path.
  until(eof(FILE)) {
	read(FILE, $buf, $EXE_HEADER_SIZE) == $EXE_HEADER_SIZE or
							die ("Short read.");
	($exe_name, $slots_used) = unpack("Z${PROCNAME_LEN}i", $buf);

	@exe = ();
	for ($i=0; $i<$slots_used; $i++) {
		read(FILE, $buf, $SLOT_SIZE) == $SLOT_SIZE or
							die ("Short read.");
		($chain, $cpath, $calls, $lo, $hi) =
			unpack("iA${CPATH_MAX_LEN}iII", $buf);

		$cycles_div_k = $hi * $UNSIGNED_MAX_DIV_K;
		$cycles_div_k += $lo / 1000;
		$ms = $cycles_div_k / $MHz;

		push @exe, [ ($cpath, $calls, $ms) ];
	}
  	$file_res{$exe_name} = [ @exe ];
  }
  return \%file_res;
}


# Aggregate calls and cycles of paths into totals for each function.
sub make_totals
{
  my $ref = shift;
  my %file_res = %{$ref};
  my $exe;
  my %res, %calls, %time;
  my @totals;

  foreach $exe (sort keys %file_res) {
	@totals = ();
	%calls = ();
	%time = ();
	@ar = @{$file_res{$exe}};
	foreach $path (@ar) {
		$_ = $path->[0];
  		s/^.* //;	# basename of call path 
		$calls{$_}	+= $path->[1];
		$time{$_}	+= $path->[2];
	}
	foreach $func (keys %calls) {
		push @totals, [ ($func, $calls{$func}, $time{$func}) ];
	}
	$res{$exe} = [ @totals ];
  }
  return \%res;
}


sub print_file
{
  my $ref = shift;
  my %file_res = %{$ref};
  my $exe;

  printf "\n========================================";
  printf "========================================\n";
  printf("Data file: %s\n", $file);
  printf "========================================";
  printf "========================================\n\n";
    
  # If we have the kernel, print it first. Then the others.
  print_exe($file_res{"kernel"}, "kernel") if exists($file_res{"kernel"});

  foreach $exe (sort keys %file_res) {
	print_exe($file_res{$exe}, $exe) unless $exe eq "kernel";
  }
}


sub print_exe
{
  my $ref = shift;
  my $name = shift;
  my @exe = @{$ref};
  my @funcs, @oldfuncs;

  my $slots_used = @exe;

  # Print a header.
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  $print_totals ? printf "%-8s  %60s functions\n", $name, $slots_used :
			printf "%-8s  %59s call paths\n", $name, $slots_used;
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  printf("%10s  %12s  path\n", "calls", "msecs");
  printf "----------------------------------------";
  printf "----------------------------------------\n";

  SWITCH: {
	if ($sort_method eq "A") {
		@exe = sort { lc($a->[0]) cmp lc($b->[0]) } @exe; last SWITCH; }
	if ($sort_method eq "C") {
		@exe = reverse sort { $a->[1] <=> $b->[1] } @exe; last SWITCH; }
	if ($sort_method eq "T") {
		@exe = reverse sort { $a->[2] <=> $b->[2] } @exe; last SWITCH; }
	last SWITCH;
  }

  my $paths;
  @oldfuncs = ();
  foreach $path (@exe) {
	printf("%10u  %12.2f  ", $path->[1], $path->[2]);

	if ($print_full_paths == 1 ||
		($sort_method eq "C" || $sort_method eq "T")) {
		print $path->[0];
	} else {
		@funcs = split(/ /, $path->[0]);
		for (my $j=0; $j<=$#funcs; $j++) {
			if ($j<=$#oldfuncs && $funcs[$j] eq $oldfuncs[$j]) {
				print " ---";
			} else {
				print " " if ($j > 0);
				print $funcs[$j];
			}
		}
		@oldfuncs = @funcs;
	}
	print "\n";
	last if (++$paths == $show_paths);
  }
  print "\n";
}

