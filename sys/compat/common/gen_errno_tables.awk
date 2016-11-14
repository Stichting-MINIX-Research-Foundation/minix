# $NetBSD: gen_errno_tables.awk,v 1.3 2005/12/11 12:19:56 christos Exp $

# Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by Christopher G. Demetriou
#      for the NetBSD Project.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Currently this script works with either gawk or nawk.
#
# Use it like:
#
#	awk -f gen_errno_tables.awk -v PREFIX=OSF1 netbsd_errno_hdr \
#	    osf1_errno_hdr
#
# It puts results into 'c' and 'h' in current directory, which can then be
# incorporated into emulation header files.
#
# Note that this script is not meant to generate perfectly pretty output.
# Wanna be a formatting weenie, hand-edit the output (or make the script
# output perfectly pretty lists).

BEGIN {
	nr_offset = 0;
	idx = 0;

	printf "" > "c"
	printf "" > "h"
}

NR != (FNR + nr_offset) {
	printf("file switch\n");
	if (idx != 0) {
		exit 1
	}
	nr_offset = (NR - FNR)
	idx = 1;
}

/^#[ \t]*define[ \t]+E[A-Z0-9]*[ \t]+[0-9]+/ {
	if ($1 == "#define") {
		name=$2
		val=$3
	} else {
		name=$3
		val=$4
	}

	if (val_max[idx] < val) {
		val_max[idx] = val;
	}
	if (mappings[idx, "val", val] == "") {
		mappings[idx, "name", name] = val
		mappings[idx, "val", val] = name
	}
}

END {
	if (idx != 1) {
		exit 1
	}

	printf("    0,\n") >> "c"
	for (i = 1; i <= val_max[0]; i++) {
		nb_name = mappings[0, "val", i]
		if (nb_name != "") {
			otheros_val = mappings[1, "name", nb_name]
			if (otheros_val != "") {
				printf("    %s_%s,\t\t/* %s (%d) -> %d */\n",
				    PREFIX, nb_name, nb_name, i,
				    otheros_val) >> "c"
			} else {
				printf("    %s_%s,\t\t/* %s (%d) has no equivalent */\n",
				    PREFIX, "ENOSYS", nb_name, i) >> "c"
			}
		}
	}

	for (i = 1; i <= val_max[1]; i++) {
		if (mappings[1, "val", i] != "") {
			printf("#define %s_%s\t\t%d\n",
			    PREFIX, mappings[1, "val", i], i) >> "h"
		}
	}
}
