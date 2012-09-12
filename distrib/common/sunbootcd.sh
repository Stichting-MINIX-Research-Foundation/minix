#! /bin/sh
#
# $NetBSD: sunbootcd.sh,v 1.6 2012/02/22 16:12:34 martin Exp $
#
# Copyright (c) 2003 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Luke Mewburn.
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

: ${SUNLABEL:=sunlabel}		# sunlabel(8)
: ${CYLSIZE:=640}		# Cylinder size, in 512byte blocks

PROGNAME=${0##*/}
FORMAT="%-8s offset %4d, size %4d, file %s\n"


usage()
{
	cat 1>&2 << _USAGE_
Usage: ${PROGNAME} fsimage sun4 [sun4c [sun4m [sun3|sun4d [sun3x|sun4u]]]]
	Combine file system partitions for Sun Microsystems, Inc. computers
	into a CD-ROM file system image suitable for booting on the
	following platforms:
		NetBSD/sun3:	sun3, sun3x
		NetBSD/sparc:	sun4, sun4c, sun4d, sun4m
		NetBSD/sparc64:	sun4u
	The architecture arguments must be bootable file system image
	for that architecture, or \`-' if no entry is desired.
	\`fsimage' is typically an iso9660 file system image, although
	any type of file system can be used as long as the first 512
	bytes of the image are not used.  \`fsimage' is modified, and
	the additional partitions are added in order.  If the same
	filename is used more than once for different architectures,
	it will only be copied once.
_USAGE_
	exit 1
}

if [ $# -lt 2 -o $# -gt 6 ]; then
	usage
fi

for curfile in $*; do
	[ "$curfile" = "-" ] && continue
	if [ ! -f "$curfile" ]; then
		echo 1>&2 "${PROGNAME}: ${curfile}: No such file."
		exit 1
	fi
done

ISOIMAGE="$1";	shift

ISOSIZE=$( ls -l "${ISOIMAGE}" | awk '{print $5}' )
ISOBLKS=$(( (${ISOSIZE} + 511) / 512 ))
ISOCYLS=$(( (${ISOBLKS} + (${CYLSIZE} - 1)) / ${CYLSIZE} ))

printf "${FORMAT}" "fsimage:" 0 ${ISOCYLS} "${ISOIMAGE}"

ENDCYL=${ISOCYLS}
curpart=0
for curfile in $*; do
	curpart=$(( ${curpart} + 1 ))
	[ "$curfile" = "-" ] && continue

	tpart=1
	curoff=${ENDCYL}
	while [ ${tpart} -lt ${curpart} ]; do
		tfile=$(eval echo \$PART${tpart}FILE)
		if [ "${curfile}" = "${tfile}" ]; then
			curoff=$(eval echo \$PART${tpart}OFF)
			break
		fi
		tpart=$(( ${tpart} + 1 ))
	done

	cursize=$( ls -l "${curfile}" | awk '{print $5}' )
	curblks=$(( (${cursize} + 511) / 512 ))
	curcyls=$(( (${curblks} + (${CYLSIZE} - 1)) / ${CYLSIZE} ))
	printf "${FORMAT}" "Image ${curpart}:" ${curoff} ${curcyls} "${curfile}"

	eval	PART${curpart}SIZE=${cursize} \
		PART${curpart}BLKS=${curblks} \
		PART${curpart}CYLS=${curcyls} \
		PART${curpart}OFF=${curoff} \
		PART${curpart}FILE="${curfile}"

	if [ $curoff -eq $ENDCYL ]; then		# append ${curfile}
		echo "    (appending ${curfile} to ${ISOIMAGE})"
		dd if="${curfile}" of="${ISOIMAGE}" bs=${CYLSIZE}b \
		    seek=${ENDCYL} conv=notrunc,sync 2>/dev/null
		ENDCYL=$(( $ENDCYL + $curcyls ))
	fi

done

printf "${FORMAT}" "Final:" 0 ${ENDCYL} "${ISOIMAGE}"

${SUNLABEL} -nq "${ISOIMAGE}" << _partinfo_
V nsect ${CYLSIZE}
V nhead 1
V rpm 300
V pcyl ${ENDCYL}
V ncyl ${ENDCYL}
a 0 $(( ${ISOCYLS} * ${CYLSIZE} ))
b ${PART1OFF:-0} $(( ${PART1CYLS:-0} * ${CYLSIZE} ))
c ${PART2OFF:-0} $(( ${PART2CYLS:-0} * ${CYLSIZE} ))
d ${PART3OFF:-0} $(( ${PART3CYLS:-0} * ${CYLSIZE} ))
e ${PART4OFF:-0} $(( ${PART4CYLS:-0} * ${CYLSIZE} ))
f ${PART5OFF:-0} $(( ${PART5CYLS:-0} * ${CYLSIZE} ))
W
_partinfo_
