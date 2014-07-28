#!/usr/local/bin/perl

$SAMPLE_SZ = 12;

$file = shift;

open (FILE, $file) or die ("Unable to open $file: $!");

$_ = <FILE>;

until (eof(FILE)) {                                 
  read(FILE, $buf, $SAMPLE_SZ) == $SAMPLE_SZ  or die ("Short read.");
  ($exe, $pc) = unpack("A8i", $buf);            

  # System and clock task are in kernel image.  
#  $exe =~ s/^system/kernel/;                    
#  $exe =~ s/^clock/kernel/;                     

  # Memory has p_name "mem" in kernel.          
#  $exe =~ s/^mem/memory/;                       

  $pcs{$pc}++ if ($exe eq "fs");
}

foreach $pc (sort { $a <=> $b } keys %pcs) {
	print "$pc $pcs{$pc}\n";
}
