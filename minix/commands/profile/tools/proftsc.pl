#!/usr/local/bin/perl

$SAMPLE_SZ = 8;

$file = shift;

open (FILE, $file) or die ("Unable to open $file: $!");

$_ = <FILE>;

until (eof(FILE)) {                                 
  read(FILE, $buf, $SAMPLE_SZ) == $SAMPLE_SZ  or die ("Short read.");
  ($high, $low) = unpack("II", $buf);            

  if ($high - $prevhigh == 0) {
	push (@res, $low - $prevlow);
  }

  $prevhigh = $high;
  $prevlow = $low;

  #print "$high $low\n";
			                                                            
#  $pcs{$pc}++ if ($exe eq "kernel");
}

foreach $diff (sort { $a <=> $b } @res) {
	print $diff."\n";
}

#foreach $pc (sort { $a <=> $b } keys %pcs) {
#	print "$pc $pcs{$pc}\n";
#}
