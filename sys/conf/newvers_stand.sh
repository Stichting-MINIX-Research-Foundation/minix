#!/bin/sh -
#
# $NetBSD: newvers_stand.sh,v 1.9 2017/04/08 19:53:54 christos Exp $
#
# Copyright (c) 2000 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jaromir Dolecek.
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

# Script for generating of vers.c file from given template. Used in
# bootblock build on various architectures.
#
# Called as:
#	sh ${S}/conf/newvers_stand.sh [-dkn] [-D <date>] [-m <machine>] VERSION_TEMPLATE [EXTRA_MSG]

cwd=$(dirname "$0")

add_name=true
add_date=true
add_kernrev=true
machine="unknown"
dateargs=

# parse command args
while getopts "m:D:dknm:" OPT; do
	case $OPT in
	D)	dateargs="-r $OPTARG";;
	d)	add_date=false;;
	k)	add_kernrev=false;;
	m)	machine=${OPTARG};;
	n)	add_name=false;;
	*)	echo "Usage: newvers_stand.sh [-dkn] [-D <date>] [-m <machine>] VERSION_TEMPLATE EXTRA_COMMENT" >&2
		exit 1;;
	esac
done

shift $(expr $OPTIND - 1)

r=$(awk -F: '$1 ~ /^[0-9.]*$/ { it = $1; } END { print it }' "$1")
shift
t=$(LC_ALL=C TZ=UTC date $dateargs)

if $add_date; then
	echo "const char bootprog_rev[] = \"${r} (${t})\";" > vers.c
else
	echo "const char bootprog_rev[] = \"${r}\";" > vers.c
fi

if $add_name; then
	extra=${1:+" $1"}

	echo "const char bootprog_name[] = \"NetBSD/${machine}${extra}\";" >> vers.c
fi

if $add_kernrev; then
	osr=$(sh "${cwd}/osrelease.sh")
	echo "const char bootprog_kernrev[] = \"${osr}\";" >> vers.c
fi
