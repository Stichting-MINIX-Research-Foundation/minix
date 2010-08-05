#!/usr/local/bin/perl
#
# sprofalyze.pl
#
# Analyzes the output files created by the profile command for
# Statistical Profiling.
#
# Changes:
#   14 Aug, 2006  Created (Rogier Meurs)
#

# Configuration options:

# Location and parameters of nm program to extract symbol tables
$nm = "/usr/bin/acknm -dn";

# Location of src (including trailing /)
	$src_root = qw(
/usr/src/
	);

# Location of system executables within src. Add new servers/drivers here.
# This should be replaced with something less maintenance-prone some day.
	@exes = qw(
kernel/kernel

servers/ds/ds
servers/hgfs/hgfs
servers/inet/inet
servers/ipc/ipc
servers/is/is
servers/iso9660fs/isofs
servers/mfs/mfs
servers/pfs/pfs
servers/pm/pm
servers/rs/rs
servers/vfs/vfs
servers/vm/vm
servers/rs/service

drivers/ahci/ahci
drivers/amddev/amddev
drivers/at_wini/at_wini
drivers/atl2/atl2
drivers/audio/es1370/es1370
drivers/audio/es1371/es1371
drivers/bios_wini/bios_wini
drivers/dec21140A/dec21140A
drivers/dp8390/dp8390
drivers/dpeth/dpeth
drivers/e1000/e1000
drivers/filter/filter
drivers/floppy/floppy
drivers/fxp/fxp
drivers/lance/lance
drivers/log/log
drivers/memory/memory
drivers/orinoco/orinoco
drivers/pci/pci
drivers/printer/printer
drivers/random/random
drivers/rtl8139/rtl8139
drivers/rtl8169/rtl8169
drivers/sb16/sb16_dsp
drivers/sb16/sb16_mixer
drivers/ti1225/ti1225
drivers/tty/tty
	);

# 8< ----------- no user configurable parameters below this line ----------- >8

$SAMPLE_SIZE = 12;
$MINIMUM_PERC = 1.0;


if ($#ARGV < 0 || process_args(@ARGV)) {
  print "Usage:\n";
  print "  sprofalyze.pl [-p percentage] file ...\n\n";
  print "    percentage  print only processes/functions >= percentage\n";
  exit 1;
}

sub process_args {
  $_ = shift;

  if (/^-p$/) {
	$_ = shift;
	return 1 unless /^(\d{1,2})(.\d+)?$/;
	$MINIMUM_PERC = $1 + $2;
  } else {
	unshift @_, $_;
  }

  @files = @_;

  return 1 unless @files > 0;

  return 0;
}

if (read_symbols()) { exit 1; }

print "Showing processes and functions using at least $MINIMUM_PERC% time.\n";

foreach $file (@files) {
  if (process_datafile($file)) { exit 1; }
}                                                    

exit 0;


sub short_name
{
  my $shortname = shift;
  $shortname =~ s/^.*\///;
  return substr($shortname, 0, 7);
}


sub read_symbols
{
  print "Building indexes from symbol tables:";

  for ($i=0; $i<= $#exes; $i++) {
	my $exe = @exes[$i];
	$shortname = $exe;
	$shortname =~ s/^.*\///;
	print " " if $i <= $#exes;
	print $shortname;
	$shortname = short_name($exe);

	$fullname = $src_root . $exe;

	if ((! -x $fullname) || (! -r $fullname)) {
		print "\nERROR: $fullname does not exist or not readable.\n";
		print "Did you do a make?\n";
		return 1;
	}

	# Create a hash entry for each symbol table (text) entry.
	foreach $_ (`$nm $fullname`) {
		if (/^0{0,7}(\d{0,8})\s[tT]\s(\w{1,8})\n$/) {
			${$shortname."_hash"}{$1} = $2;
		}
	}

	# Create hash entries for every possible pc value. This will pay off.
	@lines = sort { $a <=> $b } keys %{$shortname."_hash"};

	for ($y = 0; $y <= $#lines; $y++) {
		for ($z = @lines->[$y] + 1; $z < @lines->[$y + 1]; $z++) {
			${$shortname."_hash"}{$z} =
				${$shortname."_hash"}{@lines->[$y]}
		}
	}
  }

  # Clock and system are in kernel image but are seperate processes.
  push(@exes, "clock");
  push(@exes, "system");
  %clock_hash = %kernel_hash;
  %system_hash = %kernel_hash;

  print ".\n\n";
  return 0;
}


sub process_datafile
{
  my %res = ();
  my %merged = ();
  my $file = shift;
  my $buf, $pc, $exe, $total_system_perc;

  unless (open(FILE, $file)) {
	print "\nERROR: Unable to open $file: $!\n";
	return 0;
  }

  # First line: check file type.
  $_ = <FILE>; chomp;
  if (!/^stat$/) {
	if (/^call$/) {
		print "Call Profiling output file: ";
		print "Use cprofalyze.pl instead.\n";
	} else {
		print "Not a profiling output file.\n";
	}
	return 0;
  }      

  $file =~ s/^.*\///;

  printf "\n========================================";
  printf "========================================\n";
  printf("Data file: %s\n", $file);
  printf "========================================";
  printf "========================================\n\n";

  # Read header with total, idle, system and user hits.
  $_ = <FILE>;
  chomp;

  ($total_hits, $idle_hits, $system_hits, $user_hits) = split (/ /);

  my $system_perc	= sprintf("%3.f", $system_hits / $total_hits * 100);
  my $user_perc		= sprintf("%3.f", $user_hits / $total_hits * 100);
  my $idle_perc		= 100 - $system_perc - $user_perc;

  printf("  System process ticks: %10d (%3d%%)\n", $system_hits, $system_perc);
  printf("    User process ticks: %10d (%3d%%)", $user_hits, $user_perc);
  printf("          Details of system process\n");
  printf("       Idle time ticks: %10d (%3d%%)", $idle_hits, $idle_perc);
  printf("          samples, aggregated and\n");
  printf("                        ----------  ----");
  printf("           per process, are below.\n");
  printf("           Total ticks: %10d (100%)\n\n", $total_hits);
 
  # Read sample records from file and increase relevant counters.
  until (eof(FILE)) {
	read(FILE, $buf, $SAMPLE_SIZE) == $SAMPLE_SIZE  or die ("Short read.");
	($exe, $pc) = unpack("Z8i", $buf);

	# We can access the hash by pc because they are all in there.
       	if (!defined(${$exe."_hash"}{$pc})) {
		print "ERROR: Undefined in symbol table indexes: ";
		print "executable $exe  address $pc\n";
		print "Did you include this executable in the configuration?\n";
		return 1;
	}
	$res{$exe}{${$exe."_hash"}{$pc}} ++;
  }

  # We only need to continue with executables that had any hits.
  my @actives = ();
  foreach my $exe (@exes) {
	$exe = short_name($exe);
	next if (!exists($res{$exe}));
	push(@actives, $exe);
  }

  # Calculate number of samples for each executable and create aggregated hash.
  %exe_hits = ();
  foreach $exe (@actives) {
	foreach my $hits (values %{$res{$exe}}) {
		$exe_hits{$exe} += $hits;
	}

	foreach my $key (keys %{$res{$exe}}) {
		$merged{sprintf("%8s %8s", $exe, $key)} = $res{$exe}{$key};
	}
  }

  $total_system_perc = 0;
  # Print the aggregated results.
  process_hash("", \%merged);

  # Print results for each executable in decreasing order.
  foreach my $exe
  (reverse sort { $exe_hits{$a} <=> $exe_hits{$b} } keys %exe_hits)
  {
	process_hash($exe, \%{$res{$exe}}) if
		$exe_hits{$exe} >= $system_hits / 100 * $MINIMUM_PERC;
  }

  # Print total of processes <threshold.
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  printf("%-47s %5.1f%% of system process samples\n", 
	"processes <$MINIMUM_PERC% (not showing functions)",
	100 - $total_system_perc);
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  printf("%-47s 100.0%%\n\n", "total");

  close(FILE);
  return 0;
}


sub process_hash
{
  my $exe = shift;
  my $ref = shift;
  %hash = %{$ref};
  my $aggr = $exe eq "";

  # Print a header.
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  if ($aggr) {
	$astr_max = 55;
	$perc_hits = $system_hits / 100;
  	printf("Total system process time %46d samples\n", $system_hits);
  } else {
	$astr_max = 64;
	$perc_hits = $exe_hits{$exe} / 100;
	$total_system_perc += $exe_perc =
		sprintf("%5.1f", $exe_hits{$exe} / $system_hits * 100);
	printf("%-47s %5.1f%% of system process samples\n", $exe, $exe_perc);
  }
  printf "----------------------------------------";
  printf "----------------------------------------\n";


  # Delete functions <threshold.  Get percentage for all functions >threshold.
  my $below_thres_hits;
  my $total_exe_perc;
  foreach my $func (keys %hash)
  {
	if ($hash{$func} < $perc_hits * $MINIMUM_PERC) {
		$below_thres_hits += $hash{$func};
		delete($hash{$func});
	} else {
		$total_exe_perc += sprintf("%3.1f", $hash{$func} / $perc_hits);
	}
  }

  # Now print the hash entries in decreasing order.
  @sorted = reverse sort { $hash{$a} <=> $hash{$b} } keys %hash;

  $astr_hits = ($hash{@sorted->[0]} > $below_thres_hits ?
	$hash{@sorted->[0]} : $below_thres_hits) / $astr_max;

  foreach $func (@sorted) {
	process_line($func, $hash{$func});
  }

  # Print <threshold hits.
  my $below_thres;
  if ($aggr) { $below_thres = "         "; }
  $below_thres .= sprintf("%8s", "<" . $MINIMUM_PERC . "%");
  process_line($below_thres, $below_thres_hits, 100 - $total_exe_perc);

  # Print footer.
  printf "----------------------------------------";
  printf "----------------------------------------\n";
  printf("%-73s 100.0%%\n\n", $aggr ? "total" : $exe);
}


sub process_line
{
  my $func = shift;
  my $hits = shift;

  my $perc = $hits / $perc_hits;
  my $astr = $hits / $astr_hits;

  if ($aggr) {
	print "$func ";
  } else {
	printf("%8s ", $func);
  }
  for ($i = 0; $i < $astr_max; $i++) {
	if ($i <= $astr) {
		print "*";
	} else {
		print " ";
	}
  }
  print " ";
  if (my $rest = shift) {
	printf("%5.1f%%\n", $rest);
  } else {
	printf("%5.1f%%\n", $perc);
  }
}

