#!/bin/sh -
#
#	$NetBSD: newvers.sh,v 1.61 2014/08/03 13:14:59 justin Exp $
#
# Copyright (c) 1984, 1986, 1990, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)newvers.sh	8.1 (Berkeley) 4/20/94

# newvers.sh -- Create a "vers.c" file containing version information.
#
# The "vers.c" file in the current directory is the primary output.  It
# contains C source code with several variables containing information
# about the build.  This file is expected to be incorporated into a
# kernel, and when that kernel is booted then the information can be
# queried by the uname(8) command.
#
# Command line options:
#
#     -r                Reproducible build: Do not embed directory
#                       names, user names, time stamps, or other dynamic
#                       information into the output file.  This intended
#                       to allow two builds done at different times and
#                       even by different people on different hosts to
#                       produce identical output.
#
#     -i <id>           Use the specified string as the value of the
#                       kernel_ident variable
#
#     -n                Do not include an ELF note section in the output
#                       file.
# Environment variables:
#
#     BUILDID		If defined, ${BUILDID} is appended to the
#			default value of the kernel_ident string.
#			(If the -i command line option is used, then
#			BUILDID is not appended.)
#
#     BUILDINFO		A string to be stored in the kernel's buildinfo
#			variable.  ${BUILDINFO} may be a multi-line string,
#			and may use C-style backslash escapes.
#			Lines may be separated by either literal newlines
#			or "\n" escape sequences.
#
# Output files:
#
#     vers.c            The "vers.c" file in the current directory is
#                       the primary output.
#
#     version           The "version" file in the current directory
#                       is both an input and an output.  See the
#                       description under "Input files".
#
# Input files:
#
#     version           The "version" file in the current directory
#                       contains an integer counter, representing the
#                       number of times this script has been executed in
#                       this directory, starting with "0" if the file
#                       does not exist.  The serial number in the file
#                       is incremented after the file is read. so that
#                       the incremented serial number is an output from
#                       the present build and an input to the next build
#                       that is performed in the same directory.
#
#     copyright         The "copyright" file (in the same directory as
#                       this script itself) contains a copyright notice,
#                       which is embedded in the copyright variable in
#                       the output file.
#
#     ident             The "ident" file in the current directory is optional.
#			If this file exists, then its contents override the
#			default value of the kernel_ident string.
#
# Input from external commands:
#
#     osrelease.sh      This script is expected to print the OS revision.
#                       The result is stored in the osrelease variable.
#

# FUNCTIONS

# source_lines [input] --
#
# Convert a multi-line string to a format that's suitable for inclusion in
# C source code.  The result should look like this:
#
# "first line\n"
# "second line\n"
#
# with <backslash><letter n> inside the quotes for each line,
# literal quotation marks around each line,
# and a literal newline separating one line from the next.
#
# Input is from "$1" if that is defined, or from stdin if $1 is not defined.
#
source_lines()
{
	if [ -n "${1+set}" ]; then
		printf "%s" "$1"
	else
		cat
	fi \
	| awk '{
		# awk does not care about whether or not the last line
		# of input ends with a newline.
		# Convert <backslash> to <backslash><backslash>.
		gsub("\\\\","\\\\");
		# Convert <quote> to <backslash><quote>
		gsub("\"","\\\"");
		# Add <backslash><letter n> to the end of each line,
		# and wrap each line in double quotes.
		printf("\"%s\\n\"\n", $0);
	}'
}

# MAIN PROGRAM

if [ ! -e version ]; then
	echo 0 > version
fi

v=$(cat version)
t=$(LC_ALL=C date)
u=${USER-root}
h=$(hostname)
d=$(pwd)
cwd=$(dirname $0)
copyright="$(cat "${cwd}/copyright")"

while [ $# -gt 0 ]; do
	case "$1" in
	-r)
		# -r: Reproducible build
		rflag=true
		;;
	-i)
		# -i <id>: Use the secified string as the
		# value of the kernel_ident variable
		id="$2"
		shift
		;;
	-n)
		# -n: Do not include a ELF note section
		# in the output file.
		nflag=true
		;;
	esac
	shift
done

if [ -z "${id}" ]; then
	if [ -f ident ]; then
		id="$(cat ident)"
	else
		id=$(basename ${d})
	fi
	# Append ".${BUILDID}" to the default value of <id>.
	# If the "-i <id>" command line option was used then this
	# branch is not taken, so the command-line value of <id>
	# is used without change.
	if [ -n "${BUILDID}" ]; then
		id="${id}.${BUILDID}"
	fi
fi

osrelcmd=${cwd}/osrelease.sh

ost="NetBSD"
osr=$(sh $osrelcmd)

if [ ! -z "${rflag}" ]; then
	fullversion="${ost} ${osr} (${id})\n"
else
	fullversion="${ost} ${osr} (${id}) #${v}: ${t}\n\t${u}@${h}:${d}\n"
fi

# Convert multi-line strings to C source code.
# Also add an extra blank line to copyright.
#
copyright_source="$(printf "%s\n\n" "${copyright}" | source_lines)"
fullversion_source="$(printf "%b" "${fullversion}" | source_lines)"
buildinfo_source="$(printf "%b" "${BUILDINFO}" | source_lines)"

# Increment the serial number in the version file
echo $(expr ${v} + 1) > version

# work around escaping issues with different shells
emptyq='""'

cat << _EOF > vers.c
/*
 * Automatically generated file from $0
 * Do not edit.
 */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>

const char ostype[] = "${ost}";
const char osrelease[] = "${osr}";
const char sccs[] = "@(#)" ${fullversion_source};
const char version[] = ${fullversion_source};
const char buildinfo[] = ${buildinfo_source:-${emptyq}};
const char kernel_ident[] = "${id}";
const char copyright[] = ${copyright_source};
_EOF

[ ! -z "${nflag}" ] && exit 0

cat << _EOF >> vers.c

/*
 * NetBSD identity note.
 */
#ifdef __arm__
#define _SHT_NOTE	%note
#else
#define _SHT_NOTE	@note
#endif

#define	_S(TAG)	__STRING(TAG)
__asm(
	".section\t\".note.netbsd.ident\", \"\"," _S(_SHT_NOTE) "\n"
	"\t.p2align\t2\n"
	"\t.long\t" _S(ELF_NOTE_NETBSD_NAMESZ) "\n"
	"\t.long\t" _S(ELF_NOTE_NETBSD_DESCSZ) "\n"
	"\t.long\t" _S(ELF_NOTE_TYPE_NETBSD_TAG) "\n"
	"\t.ascii\t" _S(ELF_NOTE_NETBSD_NAME) "\n"
	"\t.long\t" _S(__NetBSD_Version__) "\n"
	"\t.p2align\t2\n"
);

_EOF
