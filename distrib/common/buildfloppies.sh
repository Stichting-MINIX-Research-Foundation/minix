#!/bin/sh
#
# $NetBSD: buildfloppies.sh,v 1.17 2008/11/12 14:22:16 apb Exp $
#
# Copyright (c) 2002-2003 The NetBSD Foundation, Inc.
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

# set defaults
#
: ${PAX=pax}
prog=${0##*/}
etcdir=/etc


usage()
{
	cat 1>&2 << _USAGE_
Usage: ${prog} [options] base size file [...]
	-i instboot	eval instboot as a shell command to install a
			bootstrap.  @IMAGE@ is replaced with with the
			file name of the floppy image.
	-m max		maximum number of floppies to build
	-N etcdir	directory in which to find passwd and group files.
	-p		pad last floppy to floppy size
	-s suffix	suffix for floppies
	base		basename of generated floppies
	size		size of a floppy in 512 byte blocks
	file [...]	file(s) to store in the floppies
_USAGE_
	exit 1
}

plural()
{
	[ $1 -ne 1 ] && echo "s"
}

roundup()
{
	echo $(( ( $1 + $2 - 1 ) / ( $2 ) ))
}


#	parse and check arguments
#

while getopts i:m:N:ps: opt; do
	case ${opt} in
	i)
		instboot=${OPTARG} ;;
	m)
		maxdisks=${OPTARG} ;;
	N)
		etcdir=${OPTARG} ;;
	p)
		pad=1 ;;
	s)
		suffix=${OPTARG} ;;
	\?|*)
		usage
		;;
	esac
done

shift $(( ${OPTIND} - 1 ))
[ $# -lt 3 ] && usage
floppybase=$1
floppysize=$2
shift 2
files=$*

#	setup temp file, remove existing images
#
floppy=floppy.$$.tar
trap "rm -f ${floppy}" 0 1 2 3			# EXIT HUP INT QUIT
rm -f ${floppybase}?${suffix}			# XXX breaks if maxdisks > 9

#	create tar file
#
dd if=/dev/zero of=${floppy} bs=8k count=1 2>/dev/null
(
	echo ". type=dir optional"
	for f in ${files}; do
		echo "./$f type=file uname=root gname=operator mode=0444"
	done
) | \
${PAX} -O -w -b8k -M -N "${etcdir}" -s,^./,, >> ${floppy} || exit 1

#	install bootstrap before the image is split into multiple disks
#
if [ -n "$instboot" ]; then
	instboot=$( echo $instboot | sed -e s/@IMAGE@/${floppy}/ )
	echo "Running instboot: ${instboot}"
	eval ${instboot} || exit 1
fi

#	check size against available number of disks
#
set -- $(ls -ln $floppy)
bytes=$5
blocks=$(roundup ${bytes} 512)
	# when calculating numdisks, take into account:
	#	a) the image already has an 8K tar header prepended
	#	b) each floppy needs an 8K tar volume header
numdisks=$(roundup ${blocks}-16 ${floppysize}-16)
if [ -z "${maxdisks}" ]; then
	maxdisks=${numdisks}
fi

#	Try to accurately summarise free space
#
msg=
# First floppy has 8k boot code, the rest an 8k 'multivolume header'.
# Each file has a 512 byte header and is rounded to a multiple of 512.
# The archive ends with two 512 byte blocks of zeros.
# The output file is then rounded up to a multiple of 8k.
# floppysize is in units of 512-byte blocks; free_space is in bytes.
free_space=$(($maxdisks * ($floppysize - 16) * 512 - 512 * 2))
for file in $files; do
	set -- $(ls -ln $file)
	file_bytes=$5
	pad_bytes=$(($(roundup $file_bytes 512) * 512 - $file_bytes))
	[ "$file_bytes" != 0 -o "$file" = "${file#USTAR.volsize.}" ] &&
		msg="$msg $file $pad_bytes,"
	free_space=$(($free_space - 512 - $file_bytes - $pad_bytes))
done
echo "Free space in last tar block:$msg"

if [ ${numdisks} -gt ${maxdisks} ]; then
	# Add in the size of the last item (we really want the kernel) ...
	excess=$(( 0 - $free_space + $pad_bytes))
	echo 1>&2 \
	    "$prog: Image is ${excess} bytes ($(( ${excess} / 1024 )) KB)"\
	    "too big to fit on ${maxdisks} disk"$(plural ${maxdisks})
	exit 1
fi

padto=$(( ${floppysize} * ${maxdisks} ))
if [ -n "${pad}" ]; then
	echo \
	    "Writing $(( ${padto} * 512 )) bytes ($(( ${padto} / 2 )) KB)" \
	    "on ${numdisks} disk"$(plural ${numdisks})"," \
	    "padded by ${free_space} bytes" \
	    "($(( ${free_space} / 1024 )) KB)"
else
	echo "Writing ${bytes} bytes ($(( ${blocks} / 2 )) KB)"\
	    "on ${numdisks} disk"$(plural ${numdisks})"," \
	    "free space ${free_space} bytes" \
	    "($(( ${free_space} / 1024 )) KB)"
fi

#	write disks
#
curdisk=1
image=
seek=0
skip=0
floppysize8k=$(( ${floppysize} / 16 ))
while [ ${curdisk} -le ${numdisks} ]; do
	image="${floppybase}${curdisk}${suffix}"
	echo "Creating disk ${curdisk} to ${image}"
	if [ ${curdisk} -eq 1 ]; then
		: > ${image}
	else
		echo USTARFS ${curdisk} > ${image}
	fi
	count=$(( ${floppysize8k} - ${seek} ))
	dd bs=8k conv=sync seek=${seek} skip=${skip} count=${count} \
	    if=${floppy} of=${image} 2>/dev/null

	curdisk=$(( ${curdisk} + 1 ))
	skip=$(( $skip + $count ))
	seek=1
done

#	pad last disk if necessary
#
if [ -n "${pad}" ]; then
	dd if=$image of=$image conv=notrunc conv=sync bs=${floppysize}b count=1
fi


#	final status
#
echo "Final result:"
ls -l ${floppybase}?${suffix}			# XXX breaks if maxdisks > 9

exit 0
