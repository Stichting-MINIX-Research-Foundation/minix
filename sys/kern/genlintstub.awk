#	$NetBSD: genlintstub.awk,v 1.10 2006/01/22 05:11:11 uwe Exp $
#
# Copyright 2001 Wasabi Systems, Inc.
# All rights reserved.
#
# Written by Perry E. Metzger for Wasabi Systems, Inc.
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
#      This product includes software developed for the NetBSD Project by
#      Wasabi Systems, Inc.
# 4. The name of Wasabi Systems, Inc. may not be used to endorse
#    or promote products derived from this software without specific prior
#    written permission.
#
# THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# This awk script is used by kernel Makefiles to construct C lint
# stubs automatically from properly formatted comments in .S files. In
# general, a .S file should have a special comment for anything with
# something like an ENTRY designation. The special formats are:
#
# /* LINTSTUB: Empty */
# This is used as an indicator that the file contains no stubs at
# all. It generates a /* LINTED */ comment to quiet lint.
#
# /* LINTSTUB: Func: type function(args); */
# type must be void, int or long. A return is faked up for ints and longs.
# Semicolon is optional.
#
# /* LINTSTUB: Var: type variable, variable; */
# This is often appropriate for assembly bits that the rest of the
# kernel has declared as char * and such, like various bits of
# trampoline code.
#
# /* LINTSTUB: include foo */
# Turns into a literal `#include foo' line in the source. Useful for
# making sure the stubs are checked against system prototypes like
# systm.h, cpu.h, etc., and to make sure that various types are
# properly declared.
#
# /* LINTSTUB: Ignore */
# This is used as an indicator to humans (and possible future
# automatic tools) that the entry is only used internally by other .S
# files and does not need a stub. You want this so you know you
# haven't just forgotten to put a stub in for something and you are
# *deliberately* ignoring it.

# LINTSTUBs are also accepted inside multiline comments, e.g.
#
# /*
#  * LINTSTUB: include <foo>
#  * LINTSTUB: include "bar"
#  */
#
# /*
#  * LINTSTUB: Func: type function(args)
#  *    Some descriptive comment about the function.
#  */

BEGIN {
	printf "/* DO NOT EDIT! DO NOT EDIT! DO NOT EDIT! */\n";
	printf "/* DO NOT EDIT! DO NOT EDIT! DO NOT EDIT! */\n";
	printf "/* This file was automatically generated. */\n";
	printf "/* see genlintstub.awk for details.       */\n";
	printf "/* This file was automatically generated. */\n";
	printf "/* DO NOT EDIT! DO NOT EDIT! DO NOT EDIT! */\n";
	printf "/* DO NOT EDIT! DO NOT EDIT! DO NOT EDIT! */\n";
	printf "\n\n";

	nerrors = 0;
}

function error(msg) {
	printf "ERROR:%d: %s: \"%s\"\n", NR, msg, $0 > "/dev/stderr";
	++nerrors;
}

END {
	if (nerrors > 0)
		exit 1;
}


# Check if $i contains semicolon or "*/" comment terminator.  If it
# does, strip them and the rest of the word away and return 1 to
# signal that no more words on the line are to be processed.

function process_word(i) {
	if ($i ~ /;/) {
		sub(";.*$", "", $i);
		return 1;
	}
	else if ($i ~ /\*\//) {
		sub("\\*\\/.*$", "", $i);
		return 1;
	}
	else if (i == NF)
		return 1;
	else
		return 0;
}


/^[\/ ]\* LINTSTUB: Func:/ {
	if (NF < 5) {
		error("bad 'Func' declaration");
		next;
	}
	if (($4 == "int") || ($4 == "long"))
		retflag = 1;
	else if ($4 == "void")
		retflag = 0;
	else {
		error("type is not int, long or void");
		next;
	}
	printf "/* ARGSUSED */\n%s", $4;
	for (i = 5; i <= NF; ++i) {
		if (process_word(i)) {
			printf " %s\n", $i;
			break;
		}
		else
			printf " %s", $i;
	}
	print "{";
	if (retflag)
		print "\treturn(0);";
	print "}\n";
	next;
}

/^[\/ ]\* LINTSTUB: Var:/ {
	if (NF < 4) {
		error("bad 'Var' declaration");
		next;
	}
	for (i = 4; i <= NF; ++i) {
		if (process_word(i)) {
			printf " %s;\n", $i;
			break;
		}
		else
			printf " %s", $i;
	}
	next;
}

/^[\/ ]\* LINTSTUB: include[ \t]+/ {
	if (NF < 4) {
		error("bad 'include' directive");
		next;
	}
	sub("\\*\\/.*$", "", $4);
	printf "#include %s\n", $4;
	next;
}

/^[\/ ]\* LINTSTUB: Empty($|[^_0-9A-Za-z])/ {
	printf "/* LINTED (empty translation unit) */\n";
	next;
}

/^[\/ ]\* LINTSTUB: Ignore($|[^_0-9A-Za-z])/ {
	next;
}

/^[\/ ]\* LINTSTUBS:/ {
	error("LINTSTUB, not LINTSTUBS");
	next;
}

/^[\/ ]\* LINTSTUB:/ {
	error("unrecognized");
	next;
}
