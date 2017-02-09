#! /usr/bin/awk -f
#	$NetBSD: podulelist2h.awk,v 1.4 2005/12/11 12:23:28 christos Exp $
#	from: devlist2h.awk,v 1.2 1996/01/22 21:08:09 cgd Exp
#
# Copyright (c) 1996 Mark Brinicombe
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
#      This product includes software developed by Mark Brinicombe
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
BEGIN {
	npodules = nvendors = 0
	dfile="podule_data.h"
	hfile="podules.h"
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)
	gsub(/ $/, "", VERSION)

	printf("/*\t\$NetBSD\$\t*/\n\n") > dfile
	printf("/*\n") > dfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > dfile
	printf(" *\n") > dfile
	printf(" * generated from:\n") > dfile
	printf(" *\t%s\n", VERSION) > dfile
	printf(" */\n") > dfile

	printf("/*\t\$NetBSD\$\t*/\n\n") > hfile
	printf("/*\n") > hfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > hfile
	printf(" *\n") > hfile
	printf(" * generated from:\n") > hfile
	printf(" *\t%s\n", VERSION) > hfile
	printf(" */\n") > hfile

	next
}
$1 == "manufacturer" {
	nvendors++

	vendorindex[$2] = nvendors;		# record index for this name, for later.
	vendors[nvendors, 1] = $2;		# name
	vendors[nvendors, 2] = $3;		# id
	printf("#define\tMANUFACTURER_%s\t%s\t", vendors[nvendors, 1],
	    vendors[nvendors, 2]) > hfile
	i = 3; f = 4;

	# comments
	ocomment = oparen = 0
	if (f <= NF) {
		printf("\t/* ") > hfile
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
		printf("%s", vendors[nvendors, i]) > hfile
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
$1 == "podule" {
	npodules++

	podules[npodules, 1] = $2;		# podule id
	podules[npodules, 2] = $3;		# id
	printf("#define\tPODULE_%s\t%s\t",
	    podules[npodules, 1], podules[npodules, 2]) > hfile

	i=3; f = 4;

	# comments
	ocomment = oparen = 0
	if (f <= NF) {
		printf("\t/* ") > hfile
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
		podules[npodules, i] = $f
		printf("%s", podules[npodules, i]) > hfile
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

	printf("static struct podule_description known_podules[] = {\n") > dfile
	for (j = 1; j <= npodules; j++) {
		printf("\t{ PODULE_%s,", podules[j, 1]) > dfile
		printf("\t\"") > dfile
		k = 3;
		needspace = 0;
		while (podules[j, k] != "") {
			if (needspace)
				printf(" ") > dfile
			printf("%s", podules[j, k]) > dfile
			needspace = 1
			k++
		}
		printf("\" },\n") > dfile
	}
	printf("\t{ 0x0000, NULL }\n") > dfile
	printf("};\n\n") > dfile

	printf("\n") > dfile

	printf("struct manufacturer_description known_manufacturers[] = {\n") > dfile
	for (i = 1; i <= nvendors; i++) {
		printf("\t{ MANUFACTURER_%s, \t", vendors[i, 1]) > dfile
		if (length(vendors[i, 1]) < 7)
			printf("\t") > dfile
		printf("\"") > dfile
		j = 3;
		needspace = 0;
		while (vendors[i, j] != "") {
			if (needspace)
				printf(" ") > dfile
			printf("%s", vendors[i, j]) > dfile
			needspace = 1
			j++
		}
		printf("\" },\n") > dfile
	}
	printf("\t{ 0, NULL }\n") > dfile
	printf("};\n") > dfile
}
