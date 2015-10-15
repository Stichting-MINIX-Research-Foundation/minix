#	$NetBSD: join.awk,v 1.6 2014/10/24 22:19:44 riz Exp $
#
# Copyright (c) 2002 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Luke Mewburn of Wasabi Systems.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# join.awk F1 F2
#	Similar to join(1), this reads a list of words from F1
#	and outputs lines in F2 with a first word that is in F1.
#	Neither file needs to be sorted

function unvis(s) \
{
	# XXX: We don't handle the complete range of vis encodings
	unvis_result = ""
	while (length(s) > 0) {
		unvis_pos = match(s, "\\\\.")
		if (unvis_pos == 0) {
			unvis_result = unvis_result "" s
			s = ""
			break
		}
		# copy the part before the next backslash
		unvis_result = unvis_result "" substr(s, 1, unvis_pos - 1)
		s = substr(s, unvis_pos)
		# process the backslash and next few chars
		if (substr(s, 1, 2) == "\\\\") {
			# double backslash -> single backslash
			unvis_result = unvis_result "\\"
			s = substr(s, 3)
		} else if (match(s, "\\\\[0-7][0-7][0-7]") == 1) {
			# \ooo with three octal digits.
			# XXX: use strnum() is that is available
			unvis_result = unvis_result "" sprintf("%c", \
				0+substr(s, 2, 1) * 64 + \
				0+substr(s, 3, 1) * 8 + \
				0+substr(s, 4, 1))
			s = substr(s, 5)
		} else {
			# unrecognised escape: keep the literal backslash
			printf "%s: %s:%s: unrecognised escape %s\n", \
				ARGV[0], (FILENAME ? FILENAME : "stdin"), FNR, \
				substr(s, 1, 2) \
				>"/dev/stderr"
			unvis_result = unvis_result "" substr(s, 1, 1)
			s = substr(s, 2)
		}
	}
	return unvis_result
}

BEGIN \
{
	if (ARGC != 3) {
		printf("Usage: join file1 file2\n") >"/dev/stderr"
		exit 1
	}
	while ( (getline < ARGV[1]) > 0) {
		$1 = unvis($1)
		words[$1] = $0
	}
	delete ARGV[1]
}

// { $1 = unvis($1) }

$1 in words \
{
	f1=$1
	$1=""
	print words[f1] $0
}
