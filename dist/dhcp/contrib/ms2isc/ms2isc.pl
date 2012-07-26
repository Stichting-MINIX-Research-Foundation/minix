#set ts=3
#
# ms2isc.pl
# MS NT4 DHCP to ISC DHCP Configuration Migration Tool
#
# Author: Shu-Min Chang
#
# Copyright(c) 2003 Intel Corporation.  All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution
# 3. Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE INTEL CORPORATION AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
# ARE DISCLAIMED.  IN NO EVENT SHALL THE INTEL CORPORATION OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR 
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO PROCUREMENT OF SUBSTITUE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVICED OF THE POSSIBILITY OF SUCH 
# DAMAGE.

use strict;
use Socket;
use Getopt::Std;
use Filehandle;
use Registry; # Custom Perl Module to make Registry access easier.

my $usage = << 'ENDOFHELP';

Purpose: A Perl Script converting MS NT4 DHCP configuration to ISC DHCP3 
configuration file by reading NT4's registry.

Requires: Registry.pm and ActiveState 5.6.0

Usage: $ARGV -s <Srv> -o <Out> [-p <Pri> [-k <key>]] [-f <Fo>]

  <Srv>  Server IP or name for NT4 DHCP server to fetch the configuration from.
  <Out>  Output filename for the configuration file.
  <Pri>  Primary DNS server name for sending the dynamic DNS update to.
  <Key>  Key name for use in updating the dynamic DNS zone.
  <Fo>   Failover peer name shared with the DHCP partner.

Essentially the <Srv> needs to be an NT4 (3.x should work but not tested) which
you should have registry read access to.  You must run this script from a 
Windows machine because of the requirement to access the registry.

The <Pri> is optional parameter for desginating the dynamic DNS update if
missing then the "zone" section of the declaration will be skipped.  The <Key>
is needed if you've configured your DNS zone with a key, in addition, you'll
need to define that key in this DHCP configuration file elsewhere manually,
read the DHCP Handbook to figure out what you need to define.

The <Fo> specifies the fail-over peer name in the pool section, you'll need to
define additional detail elsewhere manually, again read the DHCP handbook.

NOTE: the program only knows of the following global and subnet options:
        3, 6, 15, 28, 44, and 46

      If it runs into options other than the known ones, it will quit.  You
      may fix this by modifying the following procedures:
        GetGlobalOptions
        GetScopes
        PrintSubnetConfig

      In addition, the resulting subnets configuration will have the "deny 
      dynamic bootp clients" you should take them out if that's not what you 
      want :).

      Finally, as the parameter structures implied, it is assumed that you
      want the same zone primary and update key for all zones and that the
      same failover is to be applied to all the pools.  Furthermore the
      subnet zones are all assumed to be class C delineated, but if you
      happend to be delegated at the class B level, this will work fine too.

Author: Shu-Min Chang <smchang@yahoo.com>

Copyright: Please read the top of the source code

Acknowledgement:
  Brian L. King for coding help, Douglas A. Darrah for testing, and James E.
Pressley for being the DHCP reference book :).

Usage: $ARGV -s <Srv> -o <Out> [-p <Pri> [-k <key>]] [-f <Fo>]

Version: 1.0.1

ENDOFHELP

###################### Begin Main Program ####################################

	my (%opts, %GlobalOptions, %SuperScopes, %Scopes);

	### Get parameters and make sure that they meet the require/optoinal criteria
	getopts('s:o:p:k:f:', \%opts) or die $usage;
	($opts{s} and $opts{o}) or die $usage;
	if ($opts{k}) { $opts{p} or die $usage; }
	
	### Read all the registry stuff into the memory
	%GlobalOptions = GetGlobalOptions($opts{s});
	%SuperScopes = GetSuperScope($opts{s});
	%Scopes = GetScopes ($opts{s});

	### Process and print out to the output file
	my ($outfile, $i, $j, @Domains);

	$outfile = new FileHandle "> $opts{o}";
	if (!defined $outfile) {
		die "Can't open file: $opts{o}: $!";
	}

	for $i (keys %SuperScopes) {
		print $outfile "\n##############################################################\n";
		my ($Scopename) = $i;
		$Scopename =~ s/ //g;
		print $outfile "shared-network $Scopename {\n";
		foreach $j (@{$SuperScopes{$i}}) {
			PrintSubnetConfig($outfile, \%GlobalOptions, \%{$Scopes{$j}}, $j, "\t", $opts{f});
			InsertIfUnique (\@Domains, $Scopes{$j}{domain}) if exists $Scopes{$j}{domain};
			delete $Scopes{$j};
		}
		print $outfile "}\n";
		if ($opts{p} or $opts{k}) {
			foreach $j (@{$SuperScopes{$i}}) {
				PrintSubnetUpdate($outfile, $j, $opts{p}, $opts{k});
			}
		}
	}

	for $i (keys %Scopes) {
		print $outfile "\n##############################################################\n";
		PrintSubnetConfig($outfile, \%GlobalOptions, \%{$Scopes{$i}}, $i, "", $opts{f});
		if ($opts{p} or $opts{k}) { PrintSubnetUpdate($outfile, $i, $opts{p}, $opts{k}); }
		InsertIfUnique (\@Domains, $Scopes{$i}{domain}) if exists $Scopes{$i}{domain};
	}

	if ($opts{p} or $opts{k}) {
		InsertIfUnique (\@Domains, $GlobalOptions{domain}) if exists $GlobalOptions{domain};
		for $i (@Domains) {
			PrintDomainUpdate($outfile, $i, $opts{p}, $opts{k});
		}
	}

	undef ($outfile);
	print "Done.\n";
	exit();

################################## End Main Program ###########################





######################################################################
sub InsertIfUnique ($$) {
	my ($Array, $data) = @_;
# purpose: insert $data into array @{$Array} iff the data is not in there yet
# input:
#   $data: scalar data to be added to the @{$Array} if unique
#   $Array: reference of the Array to compare the uniqueness of the $data
# output:
#   $Array: reference of the array with the resulting array.
# return: none

	my ($i);

	for ($i=0; $i<=$#{$Array} && ${$Array}[$i] ne $data; $i++) { }

	if ($i > $#{$Array}) {
		${$Array}[$i] = $data;
	}
}
######################################################################
sub PrintDomainUpdate ($$$$) {
	my ($outfile, $Domain, $DDNSServer, $key) = @_;
# purpose: print out the foward domain zone update declaration
# input:
#   $outfile: filehandle of the file to write the output to
#   $Domain: a string representing the forward domain
#   $DDNSServer: a string of the DNS server accepting the DDNS update
#   $key: a string representing the key used to update the zone
# output: none
# return: none
#

	print $outfile "zone $Domain {\n";
	print $outfile "\tprimary $DDNSServer;\n";
	!$key or print $outfile "\tkey $key;\n";
	print $outfile "}\n";

}
######################################################################
sub PrintSubnetUpdate ($$$$) {
	my ($outfile, $Subnet, $DDNSServer, $key) = @_;
# purpose: print out the reverse domain zone update declaration
# input:
#   $outfile: filehandle of the file to write the output to
#   $Subnet: a string representing the subnet in the form 1.2.3.4
#   $DDNSServer: a string of the DNS server accepting the DDNS update
#   $key: a string representing the key used to update the zone
# output: none
# return: none
#

	my ($Reverse);

	$_ = join (".", reverse(split(/\./, $Subnet)));
	m/\d*\.(.*)/;
	$Reverse = $1;
	print $outfile "zone $Reverse.in-addr.arpa. {\n";
	print $outfile "\tprimary $DDNSServer;\n";
	!$key or print $outfile "\tkey $key;\n";
	print $outfile "}\n";

}
######################################################################
sub PrintSubnetConfig ($$$$$$) {
	my ($outfile, $GlobalOptions, $Scope, $Subnet, $prefix, $failover) = @_;
# purpose: print out the effective scope configuration for one subnet as
#          derived from the global and scope options.
# input:
#   $outfile: filehandle of the file to write the output to
#   $GlobalOptions: refernce to the hashed variable from GetGlobalOptions
#   $Scopes: reference to the hashed variable of the subnet in interest
#   $Subnet: string variable of the subnet being processed
#   $prefix: string to be printed before each line (designed for tab)
#   $failover: string to be used for the "failover peer" line
# output: none
# return: none
#
	my ($pound) = ( ${$Scope}{disable}? "#".$prefix : $prefix);
	print $outfile $pound, "subnet $Subnet netmask ${$Scope}{mask} {\n";
	print $outfile "$prefix# Name: ${$Scope}{name}\n";
	print $outfile "$prefix# Comment: ${$Scope}{comment}\n";
	if (exists ${$Scope}{routers}) {
		print $outfile $pound, "\toption routers @{${$Scope}{routers}};\n";
	} elsif (exists ${$GlobalOptions}{routers}) {
		print $outfile $pound, "\toption routers @{${$GlobalOptions}{routers}};\t# NOTE: obtained from global option, bad practice detected\n";
	} else {
		print $outfile "### WARNING: No router was found for this subnet!!! ##########\n";
	}
	
	if (exists ${$Scope}{dnses}) {
		print $outfile $pound, "\toption domain-name-servers ", join(",", @{${$Scope}{dnses}}), ";\n";
	} elsif (exists ${$GlobalOptions}{dnses}) {
		print $outfile $pound, "\toption domain-name-servers ", join(",", @{${$GlobalOptions}{dnses}}), ";\n";
	}

	if (exists ${$Scope}{domain}) {
		print $outfile $pound, "\toption domain-name \"${$Scope}{domain}\";\n";
	} elsif (exists ${$GlobalOptions}{domain}) {
		print $outfile $pound, "\toption domain-name \"${$GlobalOptions}{domain}\";\n";
	}

	if (exists ${$Scope}{broadcast}) {
		print $outfile $pound, "\toption broadcast-address ${$Scope}{broadcast};\n";
	} elsif (exists ${$GlobalOptions}{broadcast}) {
		print $outfile $pound, "\toption broadcast-address ${$GlobalOptions}{broadcast};\n";
	}

	if (exists ${$Scope}{winses}) {
		print $outfile $pound, "\toption netbios-name-servers ", join(",", @{${$Scope}{winses}}), ";\n";
	} elsif (exists ${$GlobalOptions}{winses}) {
		print $outfile $pound, "\toption netbios-name-servers ", join(",", @{${$GlobalOptions}{winses}}), ";\n";
	}

	if (exists ${$Scope}{winstype}) {
		print $outfile $pound, "\toption netbios-node-type ${$Scope}{winstype};\n";
	} elsif (exists ${$GlobalOptions}{winstype}) {
		print $outfile $pound, "\toption netbios-node-type ${$GlobalOptions}{winstype};\n"
	}

	print $outfile $pound, "\tdefault-lease-time ${$Scope}{leaseduration};\n";
	print $outfile $pound, "\tpool {\n";
	for (my $r=0; $r<=$#{${$Scope}{ranges}}; $r+=2) {
		print $outfile $pound, "\t\trange ${$Scope}{ranges}[$r] ${$Scope}{ranges}[$r+1];\n";
	}
	!$failover or print $outfile $pound, "\t\tfailover peer \"$failover\";\n";
	print $outfile $pound, "\t\tdeny dynamic bootp clients;\n";
	print $outfile $pound, "\t}\n";
	print $outfile $pound, "}\n";
}

######################################################################
sub GetScopes ($) {
	my ($Server) = @_;
	my (%Scopes);
# purpose: to return NT4 server's scope configuration
# input:
#   $Server: string of the valid IP or name of the NT4 server
# output: none
# return:
#   %Scope: hash of hash of hash of various data types to be returned of the 
#           following data structure
#     $Scope{<subnet>}{disable} => boolean
#     $Scope{<subnet>}{mask} => string (e.g. "1.2.3.255")
#     $Scope{<subnet>}{name} => string (e.g "Office Subnet #1")
#     $Scope{<subnet>}{comment} => string (e.g. "This is a funny subnet")
#     $Scope{<subnet>}{ranges} => array of paired inclusion IP addresses
#                                 (e.g. "1.2.3.1 1.2.3.10 1.2.3.100 10.2.3.200
#                                  says that we have 2 inclusion ranges of
#                                  1-10 and 100-200)
#     $Scopes{<subnet>}{routers} => array of IP address strings
#     $Scopes{<subnet>}{dnses} => array of IP address/name string
#     $Scopes{<subnet>}{domain} > string
#     $Scopes{<subnet>}{broadcast} => string
#     $Scopes{<subnet>}{winses} => array of IP addresses/name string
#     $Scopes{<subnet>}{winstype} => integer
#     $Scopes{<subnet>}{leaseduration} => integer

	my ($RegVal, @Subnets, @Router, $SubnetName, $SubnetComment, @SubnetOptions, @SRouter, @SDNSServers, @SDomainname, @SWINSservers, @SNetBIOS, @SLeaseDuration, @SSubnetState, @SExclusionRanges, @SSubnetAddress, @SSubnetMask, @SFirstAddress, $SStartAddress, $SEndAddress, @InclusionRanges, @SBroadcastAddress);

	print "Getting list of subnets\n";
	if (Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets", \@Subnets)) {
		die "Unable to obtain a list of subnets from the server!\n";
	}

	for (my $i=0; $i<=$#Subnets; $i++) {
		print "\t Fetching Subnet $Subnets[$i] (",$i+1, "/", $#Subnets+1, "): ";

		print ".";
		if (!Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\IpRanges", \@SFirstAddress)) {
			# Don't know why MS has a tree for this, but as far
			# as I can tell, only one subtree will ever come out of
			# this, so I'm skipping the 'for' loop
		
			print ".";
			if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\IpRanges\\$SFirstAddress[0]\\StartAddress", \$RegVal)) {
				$SStartAddress = $RegVal;
			}
			print ".";
			if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\IpRanges\\$SFirstAddress[0]\\EndAddress", \$RegVal)) {
				$SEndAddress = $RegVal;
			}
# print "\n\tInclusion Range: ", Registry::ExtractIp($SStartAddress), " - ", Registry::ExtractIp($SEndAddress),"\n";
	
		} else {
			die "\n\n# Error Getting Inclusion Range FirstAddress!!!\n\n";
		}

		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\ExcludedIpRanges", \$RegVal)) {
			@SExclusionRanges = Registry::ExtractExclusionRanges($RegVal);

#			for (my $j=2; $j<=$#SExclusionRanges; $j+=2) {
#				if (unpack("L",$SExclusionRanges[$j]) < unpack("L",$SExclusionRanges[$j-2])) {
#					print ("\n******** Subnet exclusion ranges out of order ********\n");
#				}
#			}

			@SExclusionRanges = sort(@SExclusionRanges);

#		print "\n\tExclusion Ranges: ";
#		for (my $j=0; $j<=$#SExclusionRanges; $j+=2) {
#			print "\n\t\t",Registry::ExtractIp($SExclusionRanges[$j])," - ",Registry::ExtractIp($SExclusionRanges[$j+1]);
#		}

		}
		@InclusionRanges = FindInclusionRanges ($SStartAddress, $SEndAddress, @SExclusionRanges);

		print ".";
		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetName", \$RegVal)) {
			$SubnetName = $RegVal;
#		print "\n\tSubnetName: $SubnetName";
		}

		print ".";
		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetComment", \$RegVal)) {
			$SubnetComment = $RegVal;
#		print "\n\tSubnetComment: $SubnetComment";
		}
		print ".";
		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetAddress", \$RegVal)) {
			@SSubnetAddress = Registry::ExtractIp($RegVal);
#		print "\n\tSubnetAddress: $SSubnetAddress[0]";
		}
		print ".";
		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetMask", \$RegVal)) {
			@SSubnetMask = Registry::ExtractIp($RegVal);
#		print "\n\tSubnetMask: $SSubnetMask[0]";
		}

		print ".";
		if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetState", \$RegVal)) {
			@SSubnetState = Registry::ExtractHex ($RegVal);
#		print "\n\tSubnetState = $SSubnetState[0]";
		}

		$Scopes{$Subnets[$i]}{disable} = hex($SSubnetState[0]) ? 1 : 0;
		$Scopes{$Subnets[$i]}{mask} = $SSubnetMask[0];
		$Scopes{$Subnets[$i]}{name} = $SubnetName;
		$Scopes{$Subnets[$i]}{comment} = $SubnetComment;
		for (my $r=0; $r<=$#InclusionRanges; $r++) {
			$Scopes{$Subnets[$i]}{ranges}[$r] = Registry::ExtractIp($InclusionRanges[$r]);
		}

################## Get scope options

		my (@SubnetOptionsList);

		print "\n\t\tOptions:";
		if (Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetOptions", \@SubnetOptionsList)) {
			die "Unable to get subnet options list for $Subnets[$i]!\n";
		}

		for (my $j=0; $j<=$#SubnetOptionsList; $j++) {
			print ".";
			if (!Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\Subnets\\$Subnets[$i]\\SubnetOptions\\$SubnetOptionsList[$j]\\OptionValue", \$RegVal)) {
				for ($SubnetOptionsList[$j]) {
					/003/ and do {
#						@SRouter = Registry::ExtractOptionIps($RegVal);
						$Scopes{$Subnets[$i]}{routers} = [Registry::ExtractOptionIps($RegVal)];
						last;
					};
					/006/ and do {
						@SDNSServers = Registry::ExtractOptionIps($RegVal);
						for (my $d=0; $d<=$#SDNSServers; $d++) {
							my ($ipname, $rest) = gethostbyaddr(pack("C4", split(/\./, $SDNSServers[$d])), &AF_INET);
							$Scopes{$Subnets[$i]}{dnses}[$d] = $ipname ? $ipname : $SDNSServers[$d];
		}
						last;
					};
					/015/ and do { 
						@SDomainname = Registry::ExtractOptionStrings($RegVal);
						$Scopes{$Subnets[$i]}{domain} = $SDomainname[0];
						last;
					};
					/028/ and do {
						@SBroadcastAddress = Registry::ExtractOptionIps($RegVal);
						$Scopes{$Subnets[$i]}{broadcast} = $SBroadcastAddress[0];
						last;
					};
					/044/ and do {
						@SWINSservers = Registry::ExtractOptionIps($RegVal);
						for (my $w=0; $w<=$#SWINSservers; $w++) {
							my ($ipname, $rest) = gethostbyaddr(pack("C4", split(/\./, $SWINSservers[$w])), &AF_INET);
							$Scopes{$Subnets[$i]}{winses}[$w] = $ipname ? $ipname : $SWINSservers[$w];
						}
						last;
					};
					/046/ and do {
						@SNetBIOS = Registry::ExtractOptionHex($RegVal);
						$Scopes{$Subnets[$i]}{winstype} = hex($SNetBIOS[0]);
						last;
					};
					/051/ and do {
						@SLeaseDuration = Registry::ExtractOptionHex($RegVal);
						$Scopes{$Subnets[$i]}{leaseduration} = hex($SLeaseDuration[0]);
						last;
					};
					die "This program does not recognize subnet option \#$SubnetOptionsList[$j] yet!\n"
				}
			} else {
					die "Unable to obtain option SubnetOptionsList[$j] from $Subnets[$i], most likely a registry problem!\n"
			}
		}
		print "\n";
	}

	return %Scopes;
}

######################################################################
sub FindInclusionRanges ($$@) {
	my ($StartAddress, $EndAddress, @ExclusionRanges) = @_;
# Purpose: to calculate and return the DHCP inclusion ranges out of
#          data provided by the NT4 DHCP server
# input:	$StartAddress:
#        $EndAddress:	
#        @ExclusionRanges
# output: none
# return: An arry of IP address pair representing the inclusion ranges
#         in the native registry format.
#

	my ($SA, $EA, @ER);
	$SA = unpack("L", $StartAddress);
	$EA = unpack("L", $EndAddress);
	@ER = @ExclusionRanges;
	for (my $i=0; $i<=$#ER; $i++) {
		$ER[$i] = unpack ("L", $ER[$i]);
	}

	my @InclusionRanges;


	$InclusionRanges[0] = $SA;
	$InclusionRanges[1] = $EA;

	for (my $i=0; $i<=$#ER; $i+=2) {
		if ($ER[$i] == $InclusionRanges[$#InclusionRanges-1]) {
			$InclusionRanges[$#InclusionRanges-1] = $ER[$i+1] + 1;
		}
		if ($ER[$i] > $InclusionRanges[$#InclusionRanges-1]) {
			$InclusionRanges[$#InclusionRanges] = $ER[$i]-1;
		}
		if (($ER[$i+1] > $InclusionRanges[$#InclusionRanges]) && 
		    ($ER[$i+1] != $EA)) {
			$InclusionRanges[$#InclusionRanges+1] = $ER[$i+1] + 1;
			$InclusionRanges[$#InclusionRanges+1] = $EA;
		}
		if ($InclusionRanges[$#InclusionRanges] < $InclusionRanges[$#InclusionRanges-1]) {
			$#InclusionRanges -= 2;
		}
	}

	for (my $i=0; $i<=$#InclusionRanges; $i++) {
		$InclusionRanges[$i] = pack("L", $InclusionRanges[$i]);
	#	print "Inclusion: ", Registry::ExtractIp($InclusionRanges[$i]), "\n";
	}
	return @InclusionRanges;
}

####################################################################
sub GetSuperScope ($) {
	my ($Server) = @_;
	my (%SuperScopes);
#
# purpose: gets the Superscope list from the given server
# input:
#   $Server:  string of the valid IP address or name of the NT4 server
# ouput: none
# return:
#   %SuperScopes: hash of array subnets with the following data structure
#          $SuperScopes{<SuperscopeName>} => array of sunbets
#
	my (@SuperScopeNames, @SCSubnetList);

	print "Getting Superscope list: ";
	if (!Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\SuperScope", \@SuperScopeNames)) {
		for (my $i=0; $i<=$#SuperScopeNames; $i++) {
			print ".";
			if (!Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\SuperScope\\$SuperScopeNames[$i]", \@SCSubnetList)) {
				$SuperScopes{$SuperScopeNames[$i]} = [@SCSubnetList];
			}
		}
		print "\n";
	}

	return %SuperScopes;
}

####################################################################
sub GetGlobalOptions($) {
	my ($Server) = @_;
	my (%GlobalOptions);
# purpose: to return NT4 server's global scope configuration
# input:
#   $Server: string of the valid IP or name of the NT4 server
# output: none
# return:
#   %GlobalOptions: hash of hash of various data types to be returned of the 
#           following data structure
#     $GlobalOptions{routers} => array of IP address strings
#     $GlobalOptions{dnses} => array of IP address/name string
#     $GlobalOptions{domain} > string
#     $GlobalOptions{broadcast} => string
#     $GlobalOptions{winses} => array of IP addresses/name string
#     $GlobalOptions{winstype} => integer

	my ($RegVal, @temp, @GlobalOptionValues);

	print "Getting Global Options: ";
	if (Registry::GetRegSubkeyList ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\Configuration\\GlobalOptionValues", \@GlobalOptionValues)) { 
		die "Unable to obtain GlobalOptionValues"; 
	}
	
	for (my $i=0; $i<=$#GlobalOptionValues; $i++) {
		print ".";
		if (Registry::GetRegKeyVal ("\\\\$Server\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\DHCPServer\\configuration\\globaloptionvalues\\$GlobalOptionValues[$i]\\optionvalue", \$RegVal)) { 
			die "Unable to retrive global option $GlobalOptionValues[$i]\n";
		}
	
	
		for ($GlobalOptionValues[$i]) {
			/003/ and do {
				@temp=Registry::ExtractOptionIps($RegVal);
				$GlobalOptions{routers} = [@temp];
				last;
			};
			/006/ and do {
				# DNS Servers
				@temp = Registry::ExtractOptionIps($RegVal);
				for (my $d=0; $d<=$#temp; $d++) {
					my ($ipname, $rest) = gethostbyaddr(pack("C4", split(/\./, $temp[$d])), &AF_INET);
					$GlobalOptions{dnses}[$d] = $ipname ? $ipname : $temp[$d];
				}
				last;
			};
			/015/ and do { 
				# Domain Name
				@temp = Registry::ExtractOptionStrings($RegVal);
				$GlobalOptions{domain} = $temp[0];
				last;
			};
			/028/ and do { 
				# broadcast address
				@temp = Registry::ExtractOptionIps($RegVal);
				$GlobalOptions{broadcast} = $temp[0];
				last;
			};
			/044/ and do {
				# WINS Servers
				@temp = Registry::ExtractOptionIps ($RegVal);
				$GlobalOptions{winses} = [@temp];
				for (my $w=0; $w<=$#temp; $w++) {
					my ($ipname, $rest) = gethostbyaddr(pack("C4", split(/\./, $temp[$w])), &AF_INET);
					$GlobalOptions{winses}[$w] = $ipname ? $ipname : $temp[$w];
				}
				last;
			};
			/046/ and do {
				# NETBIOS node type
				@temp = Registry::ExtractOptionHex($RegVal);
				$GlobalOptions{winstype} = hex($temp[0]);
				last;
			};
			die "This program does not recgonize global option \#$GlobalOptionValues[$i] yet!\n"
		}
	}
	print "\n";

	return %GlobalOptions;
}
