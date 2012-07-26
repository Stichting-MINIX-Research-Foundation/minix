# Registry.pm
#   A perl module provided easy Windows Registry access
#
# Author: Shu-Min Chang
#
# Copyright(c) 2002 Intel Corporation.  All rights reserved
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

package Registry;
use strict;
use Win32API::Registry 0.21 qw( :ALL );


###############################################################################

#-----------------------------------------
sub GetRegKeyVal($*) {
	my ($FullRegPath, $value) = @_;
#-----------------------------------------
# Purpose: uses Win32API to get registry information from a given server
#
# WARNING: this procedure is VERY Win32 specific, you'll need a Win32 manual
#          to figure out why something is done.
# input: $FullRegPath: a MS specific way of fully qualifying a registry path
#                     \\Server\RootKey\Path\ValueName
# output: *value: the value of the registry key of $FullRegPath
#

	my ($RemoteMachine, $RootKey, $RegPath, $KeyName, $i);

#print "in sub:GetRegKeyVal:Parameters:", @_, "\n";

	# Check the for valid fully qualified registry path
	return -1 if (! ($FullRegPath =~ /\\.+\\.+/)) && (!($FullRegPath =~ /\\\\.+\\.+\\.+/));


	$RemoteMachine = (index($FullRegPath, "\\\\") == $[ ? substr($FullRegPath, $[+2, index($FullRegPath, "\\", $[+2)-2):0);

#print "RemoteMachine = $RemoteMachine\n";

	$i = $RemoteMachine ? $[+3+length($RemoteMachine) : $[+1;
	$RootKey = substr ($FullRegPath, $i, index($FullRegPath, "\\", $i)-$i);

	$KeyName = $FullRegPath;
	$KeyName =~ s/.*\\(.+)/$1/;
#print "KeyName = $KeyName\n";

	$i = index($FullRegPath, $RootKey, $[+length($RemoteMachine)) + $[ + length($RootKey)+1;
	$RegPath = substr ($FullRegPath, $i, length($FullRegPath) - length($KeyName) -$i - 1);
#print "RegPath = $RegPath\n";

	my ($RootKeyHandle, $handle, $key, $type);

  if ($RemoteMachine) {
		$RootKeyHandle = regConstant($RootKey);

		if (!RegConnectRegistry ($RemoteMachine, $RootKeyHandle, $handle)) {
			$$value = regLastError();
			return -2;
		}
	} else { # not valid actually because I can't find the mapping table of default 
            # local handle mapping.  Should always pass in the Machine name to use for now
		$handle = $RootKey;
	}

	if (!RegOpenKeyEx ($handle, $RegPath, 0, KEY_READ, $key)) {
		$$value = regLastError();
#print "regLastError = $$value\n";
		return -3;
	}
	if (!RegQueryValueEx( $key, $KeyName, [], $type, $$value, [] )) {
		$$value = regLastError();
#print "regLastError = $$value\n";
		return -4;
	}

#print "RegType=$type\n";	# Perl doesn't fetch type, at this in this 
				# ActiveState 5.6.0 that I'm using
#print "RegValue=$$value\n";
	RegCloseKey ($key);
	RegCloseKey ($handle);

	return 0;
}

###############################################################################

#-----------------------------------------
sub GetRegSubkeyList($*) {
	my ($FullKeyRegPath, $Subkeys) = @_;
#-----------------------------------------
# Purpose: uses Win32API to get registry subkey list from a given server
#
# WARNING: this procedure is VERY Win32 specific, you'll need a Win32 manual
#          to figure out why something is done.
# input: $FullKeyRegPath: a MS specific way of fully qualifying a registry path
#                     \\Server\RootKey\Path\KeyName
# output: *Subkeys: the list of subkeys in array of the registry key of 
#                   $FullKeyRegPath
#

	my ($RemoteMachine, $RootKey, $RegPath, $KeyName, $i);

#print "in sub:GetRegSubkeyList:Parameters:", @_, "\n";

	# Check the for valid registry key path
	return -1 if (! ($FullKeyRegPath =~ /\\.+\\.+/)) && (!($FullKeyRegPath =~ /\\\\.+\\.+\\.+/));


	$RemoteMachine = (index($FullKeyRegPath, "\\\\") == $[ ? substr($FullKeyRegPath, $[+2, index($FullKeyRegPath, "\\", $[+2)-2):0);

#print "RemoteMachine = $RemoteMachine\n";

	$i = $RemoteMachine ? $[+3+length($RemoteMachine) : $[+1;
	$RootKey = substr ($FullKeyRegPath, $i, index($FullKeyRegPath, "\\", $i)-$i);

	$i = index($FullKeyRegPath, $RootKey, $[+length($RemoteMachine)) + $[ + length($RootKey)+1;
	$RegPath = substr ($FullKeyRegPath, $i);

#print "RegPath = $RegPath\n";

	my ($RootKeyHandle, $handle, $key, $type);

	if ($RemoteMachine) {
		$RootKeyHandle = regConstant($RootKey);

		if (!RegConnectRegistry ($RemoteMachine, $RootKeyHandle, $handle)) {
			@$Subkeys[0]= regLastError();
			return -2;
		}
	} else { # not valid actually because I can't find the mapping table of default 
            # local handle mapping.  Should always pass in the Machine name to use for now
		$handle = $RootKey;
	}

	if (!RegOpenKeyEx ($handle, $RegPath, 0, KEY_READ, $key)) {
		@$Subkeys[0] = regLastError();
#print "regLastError = @$Subkeys[0]\n";
		return -3;
	}

	my $tmp;
	# For some reason, the regLastError() stays at ERROR_NO_MORE_ITEMS
	# in occasional call sequence, so I'm resetting the error code
	# before entering the loop
	regLastError(0);
	for ($i=0; regLastError()==regConstant("ERROR_NO_MORE_ITEMS"); $i++) {
#print "\nERROR: error enumumerating reg\n";
		if (RegEnumKeyEx ($key, $i, $tmp, [], [], [], [], [])) {
			@$Subkeys[$i] = $tmp;
		}
	}
	
#print "RegType=$type\n";
#print "RegValue=@$Subkeys\n";
	RegCloseKey ($key);
	RegCloseKey ($handle);

	return 0;
}

#####################################################

sub ExtractOptionIps ($) {
	my ($MSDHCPOption6Value) = @_;
	my @ip;
# purpose: DHCP registry specific; to return the extracted IP addresses from 
#          the input variable
# input:
#   $MSDHCPOption6Value: Option 6 was used to develop, but it works for any
#                        other options of the same datatype.
# output: none
# return: 
#   @ip: an arry of IP addresses in human readable format.


	# First extract the size of the option
	my ($byte, $size, $ind1, $ind2, @octet) = unpack("VVVV", $MSDHCPOption6Value);
# print "byte = $byte\nsize=$size\nind1=$ind1\nind2=$ind2\n";

	# Calculate total number of bytes that IP addresses occupy
	my $number = $size * $ind1;
	($byte, $size, $ind1, $ind2, @octet) = unpack("VVVVC$number", $MSDHCPOption6Value);

	for (my $i=0; $i<$#octet; $i=$i+4) {
		$ip[$i/4] = "$octet[$i+3]\.$octet[$i+2]\.$octet[$i+1]\.$octet[$i]";
	}

	return @ip;
}

#####################################################

sub ExtractOptionStrings ($) {
	my ($MSDHCPOption15Value) = @_;
	my @string;
# purpose: DHCP registry specific; to return the extracted string from 
#          the input variable
# input:
#   $MSDHCPOption15Value: Option 15 was used to develop, but it works for any
#                         other options of the same datatype.
# output: none
# return: 
#   @string: an arry of strings in human readable format.


	# First extract the size of the option
	my ($byte, $start, $ind1, $ind2, $size, @data) = unpack("VVVVV", $MSDHCPOption15Value);
# print "byte = $byte\nstart=$start\nind1=$ind1\nind2=$ind2\nsize=$size\n";

	# Calculate total number of bytes that IP addresses occupy
	my $number = $size * $ind1;
	($byte, $start, $ind1, $ind2, $size, @data) = unpack("VVVVVC$number", $MSDHCPOption15Value);

	for (my $i=0; $i<$ind1; $i++) {
	# actually this is only programmed to do one string, until I see
	# example of how the multiple strings are represented, I don't have a
	# guess to how to program them properly.
		for (my $j=0; $j<$#data & $data[$j]!=0; $j+=2) {
			$string[$i] = $string[$i].chr($data[$j]);
		}
	}

	return @string;
}

#####################################################

sub ExtractOptionHex ($) {
	my ($MSDHCPOption46Value) = @_;
	my @Hex;
# purpose: DHCP registry specific; to return the extracted hex from the input
#          variable
# input:
#   $MSDHCPOption46Value: Option 46 was used to develop, but it works for any
#                         other options of the same datatype.
# output: none
# return: 
#   @Hex: an arry of hex strings in human readable format.
	my $Temp;


	# First extract the size of the option
	my ($byte, $unknown, $ind1, $ind2, @data) = unpack("VVVV", $MSDHCPOption46Value);
# print "byte=$byte\nunknown=$unknown\nind1=$ind1\nind2=$ind2\n";

	# Calculate total number of bytes that IP addresses occupy
	my $number = $byte - 15;
	($byte, $unknown, $ind1, $ind2, @data) = unpack("VVVVC$number", $MSDHCPOption46Value);

# printf "data=%4x\n", $data[0];

	for (my $i=0; $i<$ind1; $i++) {
	# actually this is only programmed to do one Hex, until I see
	# example of how the multiple Hexes are represented, I don't have a
	# guess to how to program them properly.
		for (my $j=3; $j>=0; $j--) {
			$Hex[$i] = $Hex[$i].sprintf ("%x", $data[$j+$i*4]);
		}
	}

	return @Hex;
}

#####################################################

sub ExtractExclusionRanges ($) {
	my ($MSDHCPExclusionRanges) = @_;
	my @RangeList;
# purpose: DHCP registry specific; to return the extracted exclusion ranges 
#          from the input variable
# input:
#   $MSDHCPExclusionRanges: Exclusion range as DHCP server returns them
# output: none
# return: 
#   @RangeList: an arry of paird IP addresses strings in human readable format.


	# First extract the size of the option
	my ($paircount, @data) = unpack("V", $MSDHCPExclusionRanges);
# print "paircount = $paircount\n";

	# Calculate total number of bytes that IP addresses occupy
#	my $number = $paircount * 4*2;
#	($paircount, @data) = unpack("VC$number", $MSDHCPExclusionRanges);
#
#	for (my $i=0; $i<$#data; $i=$i+4) {
#		$ip[$i/4] = "$data[$i+3]\.$data[$i+2]\.$data[$i+1]\.$data[$i]";
#	}
#
	my $number = $paircount * 2;
	($paircount, @data) = unpack("VL$number", $MSDHCPExclusionRanges);

	for (my $i=0; $i<=$#data; $i++) {
		$RangeList[$i] = pack ("L", $data[$i]);
# print "extracted", ExtractIp ($RangeList[$i]), "\n";
	}

	return @RangeList;
}
#####################################################

sub ExtractIp ($) {
	my ($octet) = @_;
# purpose: to return the registry saved IP address in a readable form
# input:
#   $octet: a 4 byte data storing the IP address as the registry save it as
# output: none
# return: anonymous variable of a string of IP address

	my (@data) = unpack ("C4", $octet);

	return "$data[3]\.$data[2]\.$data[1]\.$data[0]";

}
#####################################################

sub ExtractHex ($) {
	my ($HexVal) = @_;
	my @Hex;
# purpose: to return the registry saved hex number in a readable form
# input:
#   $octet: a 4 byte data storing the hex number as the registry save it as
# output: none
# return: 
#   $Hex: string of hex digit


	# First extract the size of the option
	my (@data) = unpack("C4", $HexVal);

	for (my $i=3; $i>=0; $i--) {
		$Hex[0] = $Hex[0] . sprintf ("%x", $data[$i]);
	}

	return @Hex;
}
1;
