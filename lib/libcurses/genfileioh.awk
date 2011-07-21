#!/usr/bin/awk -F
#
#	$NetBSD: genfileioh.awk,v 1.2 2008/05/02 11:13:02 martin Exp $
#
# Copyright (c) 2008 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Julian Coleman.
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

function mangle_vers(vers) {
	sub("^.*\\$NetBSD", "NetBSD", vers);
	sub("\\$[^$]*$", "", vers);
	return vers;
}

BEGIN {
	MYVER="$NetBSD: genfileioh.awk,v 1.2 2008/05/02 11:13:02 martin Exp $";
	MYVER=mangle_vers(MYVER);
}

{
	if ($0 ~/\$NetBSD:/) {
		SHVER=mangle_vers($0);
	}
	if ($1 ~ /^major=/) {
		MAJ=$1;
		sub("^major=", "", MAJ);
	}
	if ($1 ~ /^minor=/) {
		MIN=$1;
		sub("^minor=", "", MIN);
	}
}

END {
	printf("/*\n");
	printf(" * Do not edit!  Automatically generated file:\n");
	printf(" *   from: %s\n", SHVER);
	printf(" *   by  : %s\n", MYVER);
	printf(" */\n");
	printf("\n");
	printf("#define CURSES_LIB_MAJOR %s\n", MAJ);
	printf("#define CURSES_LIB_MINOR %s\n", MIN);
}
