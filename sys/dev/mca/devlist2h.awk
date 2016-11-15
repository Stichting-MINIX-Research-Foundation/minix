#! /usr/bin/awk -f
#	$NetBSD: devlist2h.awk,v 1.4 2005/12/11 12:22:18 christos Exp $
#
# Copyright (c) 1995, 1996 Christopher G. Demetriou
# All rights reserved.
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
#      This product includes software developed by Christopher G. Demetriou.
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
#

# Adapted for MCA needs by Jaromir Dolecek.

BEGIN {
	nproducts = nvendors = blanklines = 0
	dfile="mcadevs_data.h"
	hfile="mcadevs.h"
	FS=" "
	alias=""
	id=""
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)
	gsub(/ $/, "", VERSION)

	printf("/*\t$NetBSD" "$\t*/\n\n") > dfile
	printf("/*\n") > dfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > dfile
	printf(" *\n") > dfile
	printf(" * generated from:\n") > dfile
	printf(" *\t%s\n", VERSION) > dfile
	printf(" */\n") > dfile

	printf("/*\t$NetBSD" "$\t*/\n\n") > hfile
	printf("/*\n") > hfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > hfile
	printf(" *\n") > hfile
	printf(" * generated from:\n") > hfile
	printf(" *\t%s\n", VERSION) > hfile
	printf(" */\n") > hfile

	next
}

NF > 0 && $1 == "product" {
	nproducts++

	alias = $3
	id = $2

	products[nproducts, 1] = $2;		# product id

	# get name - it's enclosed in parenthesis
	sub("[^\"]*\"", "")
	sub("\".*", "")
	products[nproducts, 2] = $0;		# name

	# if third parameter is an alias, print appropriate entry to hfile,
	# otherwise just store the entry for later processing
	if (substr(alias, 1, 1) != "\"") {
		printf("#define\tMCA_PRODUCT_%s\t%s\t/* %s */\n", alias, id,\
			$0) > hfile
	}

}
{
	if ($0 == "")
		blanklines++
	if (blanklines < 2) {
		print $0 > hfile
		print $0 > dfile
	}
	else if (blanklines == 2) {
		printf("\n/*\n * List of known MCA devices\n */\n\n") > dfile
		printf("\n/*\n * Supported MCA devices\n */\n\n") > hfile
	}

	next
}
END {
	# print out the match tables

	printf("const struct mca_knowndev mca_knowndevs[] = {\n") > dfile
	for (i = 1; i <= nproducts; i++) {
		printf("    { %s,\t\"%s\" },\n", products[i, 1],
			products[i, 2]) > dfile
	}
	printf("    { 0, NULL, }\n") > dfile
	printf("};\n") > dfile
	close(dfile)
	close(hfile)
}
