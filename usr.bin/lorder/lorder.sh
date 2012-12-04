#!/bin/sh -
#	$NetBSD: lorder.sh,v 1.15 2007/01/14 16:29:35 apb Exp $
#
# Copyright (c) 1990, 1993
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
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
#	@(#)lorder.sh	8.1 (Berkeley) 6/6/93
#

# If the user has set ${NM} then we use it, otherwise we use 'nm'.
# Similarly for JOIN, MKTEMP, SED, and SORT.
#
# For each of these, we try to find the command in the user's path, and
# if that fails we try to find it in the default path.  If we can't
# find it, we punt.  Once we find it, we canonicalize its name and set
# the path to the default path so that other commands we use are picked
# properly.

: ${JOIN:=join}
: ${MKTEMP:=mktemp}
: ${NM:=nm}
: ${SED:=sed}
: ${SORT:=sort}
for var in JOIN MKTEMP NM SED SORT ; do
	if ! eval type "\$${var}" >/dev/null 2>&1 ; then
		PATH=/bin:/usr/bin
		export PATH
		if ! eval type "\$${var}" > /dev/null 2>&1; then
			eval echo "lorder: \$${var}: not found" >&2
			exit 1
		fi
	fi
	cmd='set $(eval type "\$${var}") ; eval echo \$$#'
	eval "${var}=\$(eval \${cmd})"
done

# only one argument is a special case, just output the name twice
case $# in
	0)
		echo "usage: lorder file ..." >&2;
		exit ;;
	1)
		echo $1 $1;
		exit ;;
esac

# temporary files
N=$("${MKTEMP}" /tmp/_nm_.XXXXXX) || exit 1
R=$("${MKTEMP}" /tmp/_reference_.XXXXXX) || exit 1
S=$("${MKTEMP}" /tmp/_symbol_.XXXXXX) || exit 1

# remove temporary files on exit
trap "rm -f $N $R $S; exit 0" 0
trap "rm -f $N $R $S; exit 1" HUP INT QUIT PIPE TERM 2>/dev/null || \
	trap "rm -f $N $R $S; exit 1" 1 2 3 13 15

# if the line ends in a colon, assume it's the first occurrence of a new
# object file.  Echo it twice, just to make sure it gets into the output.
#
# if the line has " T " or " D " it's a globally defined symbol, put it
# into the symbol file.
#
# if the line has " U " it's a globally undefined symbol, put it into
# the reference file.
(for file in $* ; do echo $file":" ; done ; $NM -go $*) >$N
"${SED}" -ne '/:$/{s/://;s/.*/& &/;p;}' <$N
"${SED}" -ne 's/:.* [TDGR] / /p' <$N >$S
"${SED}" -ne 's/:.* U / /p' <$N >$R

# sort symbols and references on the second field (the symbol)
# join on that field, and print out the file names.
"${SORT}" -k2 $R -o $R
"${SORT}" -k2 $S -o $S
"${JOIN}" -j 2 -o 1.1,2.1 $R $S
