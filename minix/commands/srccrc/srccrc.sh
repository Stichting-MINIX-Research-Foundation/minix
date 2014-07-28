#!/bin/sh
#
#	srccrc 1.0 - compute CRC checksums of the entire source tree
#							Author: Kees J. Bot
cd /usr || exit

{
	# List the file names of all files in /usr/include and /usr/src.
	find include src/* -type f
} | {
	# Sort the list to make them comparable.
	sort
} | {
	# Remove files like *.o, *.bak, etc.
	sed -e '/\.o$/d
		/\.a$/d
		/\.bak$/d
		/\/a\.out$/d
		/\/core$/d
		/\/bin\/[^/]*$/d'
} | {
	# Compute checksums.
	xargs crc
}
