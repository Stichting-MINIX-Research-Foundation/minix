#! /usr/bin/awk -f
#	$NetBSD: devlist2h.awk,v 1.1 2014/09/21 14:30:22 christos Exp $
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
NR == 1 {
	nproducts = nvendors = blanklines = 0
	nchars = 1
	dfile= FILENAME "_data.h"
	hfile= FILENAME ".h"
	prefix = FILENAME
	gsub("devs", "", prefix)
	PREFIX = toupper(prefix)
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
NF > 0 && $1 == "vendor" {
	nvendors++

	vendorindex[$2] = nvendors;		# record index for this name, for later.
	vendors[nvendors, 1] = $2;		# name
	vendors[nvendors, 2] = $3;		# id
	printf("#define\t%s_VENDOR_%s\t%s", PREFIX, vendors[nvendors, 1],
	    vendors[nvendors, 2]) > hfile

	i = 3; f = 4;

	# comments
	ocomment = oparen = 0
	if (f <= NF) {
		printf("\t\t/* ") > hfile
		ocomment = 1;
	}
	while (f <= NF) {
		if ($f == "#") {
			printf("(") > hfile
			oparen = 1
			f++
			continue
		}
		if (oparen) {
			printf("%s", $f) > hfile
			if (f < NF)
				printf(" ") > hfile
			f++
			continue
		}
		vendors[nvendors, i] = $f
		if (words[$f, 1] == 0) {
			l = length($f);
			parts = split($f, junk, "\\");
			l = l - (parts - 1);
			nwords++;
			words[$f, 1] = nwords;
			words[$f, 2] = l;
			wordlist[nwords, 1] = $f;
			wordlist[nwords, 3] = nchars;
			nchars = nchars + l + 1;
		}
		wordlist[words[$f, 1], 2]++;
		vendors[nvendors, i] = words[$f, 1];
		printf("%s", $f) > hfile
		if (f < NF)
			printf(" ") > hfile
		i++; f++;
	}
	if (oparen)
		printf(")") > hfile
	if (ocomment)
		printf(" */") > hfile
	printf("\n") > hfile

	next
}
NF > 0 && $1 == "product" {
	nproducts++

	products[nproducts, 1] = $2;		# vendor name
	products[nproducts, 2] = $3;		# product id
	products[nproducts, 3] = $4;		# id
	printf("#define\t%s_PRODUCT_%s_%s\t%s", PREFIX, products[nproducts, 1],
	    products[nproducts, 2], products[nproducts, 3]) > hfile

	i=4; f = 5;

	# comments
	ocomment = oparen = 0
	if (f <= NF) {
		printf("\t\t/* ") > hfile
		ocomment = 1;
	}
	while (f <= NF) {
		if ($f == "#") {
			printf("(") > hfile
			oparen = 1
			f++
			continue
		}
		if (oparen) {
			printf("%s", $f) > hfile
			if (f < NF)
				printf(" ") > hfile
			f++
			continue
		}
		if (words[$f, 1] == 0) {
			l = length($f);
			parts = split($f, junk, "\\");
			l = l - (parts - 1);
			nwords++;
			words[$f, 2] = l;
			words[$f, 1] = nwords;
			wordlist[nwords, 1] = $f;
			wordlist[nwords, 3] = nchars;
			nchars = nchars + l + 1;
		}
		wordlist[words[$f, 1], 2]++;
		products[nproducts, i] = words[$f, 1];
		printf("%s", $f) > hfile
		if (f < NF)
			printf(" ") > hfile
		i++; f++;
	}
	if (oparen)
		printf(")") > hfile
	if (ocomment)
		printf(" */") > hfile
	printf("\n") > hfile

	next
}
{
	if ($0 == "")
		blanklines++
	print $0 > hfile
	if (blanklines < 2)
		print $0 > dfile
}
END {
	# print out the match tables

	printf("\n") > dfile

	printf("static const uint16_t %s_vendors[] = {\n", prefix) > dfile
	for (i = 1; i <= nvendors; i++) {
		printf("\t    %s_VENDOR_%s", PREFIX, vendors[i, 1]) \
		    > dfile

		j = 3;
		while ((i, j) in vendors) {
			printf(", %d",
			    wordlist[vendors[i, j], 3]) > dfile
#			printf(", %d /* %s */",
#			    wordlist[vendors[i, j], 3],
#			    wordlist[vendors[i, j], 1]) > dfile
			j++
		}
		printf(", 0,\n", sep) > dfile
	}
	printf("};\n") > dfile

	printf("\n") > dfile

	printf("static const uint16_t %s_products[] = {\n", prefix) > dfile
	for (i = 1; i <= nproducts; i++) {
		printf("\t    %s_VENDOR_%s, %s_PRODUCT_%s_%s, \n",
		    PREFIX, products[i, 1], PREFIX, products[i, 1],
		    products[i, 2]) > dfile

		printf("\t    ") > dfile
		j = 4
		sep = ""
		while ((i, j) in products) {
			printf("%s%d", sep,
			    wordlist[products[i, j], 3]) > dfile
#			printf("%s%d /* %s */", sep,
#			    wordlist[products[i, j], 3],
#			    wordlist[products[i, j], 1]) > dfile
			sep = ", "
			j++
		}
		printf("%s0,\n", sep) > dfile
	}
	printf("};\n") > dfile

	printf("static const char %s_words[] = { \".\" \n", prefix) > dfile
	for (i = 1; i <= nwords; i++) {
		printf("\t    \"%s\\0\" /* %d refs @ %d */\n",
		    wordlist[i, 1], wordlist[i, 2], wordlist[i, 3]) > dfile
	}
	printf("};\n") > dfile
	printf("const int %s_nwords = %d;\n", prefix, nwords) > dfile

	printf("\n") > dfile

	close(dfile)
	close(hfile)
}
