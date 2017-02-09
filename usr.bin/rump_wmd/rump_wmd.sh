#!/bin/sh
#
#	$NetBSD: rump_wmd.sh,v 1.4 2014/01/28 13:58:25 pooka Exp $
#
# Copyright (c) 2014 Antti Kantee <pooka@iki.fi>
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

: ${CC:=cc}
DEBUGLEVEL=0
LIBDIR=/usr/lib

die ()
{

	echo $* >&2
	exit 1
}

usage ()
{

	die "Usage: $0 [-hv] [-L libdir] -lrump_component [...]"
}

unset FIRSTLIB
while getopts 'hl:L:v' opt; do
	case "${opt}" in
	l)
		: ${FIRSTLIB:=${OPTIND}}
		;;
	L)
		[ -z "${FIRSTLIB}" ] || usage
		LIBDIR=${OPTARG}
		;;
	v)
		[ -z "${FIRSTLIB}" ] || usage
		DEBUGLEVEL=$((DEBUGLEVEL + 1))
		;;
	-h|*)
		usage
		;;
	esac
done
[ -z "${FIRSTLIB}" ] && usage
shift $((${FIRSTLIB} - 2))
[ $# -eq 0 ] && usage

debug ()
{

	[ ${DEBUGLEVEL} -ge ${1} ] && \
	    { lvl=$1; shift; echo DEBUG${lvl}: $* >&2; }
}

# filters from list
filter ()
{

	filtee=$1
	vname=$2
	tmplist=''
	found=false
	for x in $(eval echo \${${vname}}); do
		if [ "${filtee}" = "${x}" ]; then
			found=true
		else
			tmplist="${tmplist} ${x}"
		fi
	done
	${found} || die \"${1}\" not found in \$${2}

	eval ${vname}="\${tmplist}"
}

SEEDPROG='int rump_init(void); int main() { rump_init(); }'
CCPARAMS='-Wl,--no-as-needed -o /dev/null -x c -'

# sanity-check
for lib in $* ; do
	[ "${lib#-l}" = "${lib}" -o -z "${lib#-l}" ] \
	    && die Param \"${lib}\" is not of format -llib
done

# starting set and available components
WANTEDCOMP="$*"
RUMPBASE='-lrump -lrumpuser'
CURCOMP="${WANTEDCOMP}"
NEEDEDCOMP=''
ALLCOMP=$(ls ${LIBDIR} 2>/dev/null \
    | sed -n '/^librump.*.so$/{s/lib/-l/;s/\.so//p;}')
[ -z "${ALLCOMP}" ] && die No rump kernel components in \"${LIBDIR}\"

# filter out ones we'll definitely not use
for f in ${CURCOMP} -lrumphijack -lrumpclient; do
	filter ${f} ALLCOMP
done

# put the factions first so that they'll be tried first.
# this is an optimization to minimize link attempts.
FACTIONS='-lrumpvfs -lrumpnet -lrumpdev'
for f in ${FACTIONS}; do
	filter ${f} ALLCOMP
done
ALLCOMP="${FACTIONS} ${ALLCOMP}"

debug 0 Searching component combinations.  This may take a while ...
while :; do
	debug 1 Current components: ${CURCOMP}

	IFS=' '
	out=$(echo ${SEEDPROG} \
	    | ${CC} -L${LIBDIR} ${CCPARAMS} ${CURCOMP} ${RUMPBASE} 2>&1)
	[ -z "${out}" ] && break
	undef=$(echo ${out} \
	    | sed -n '/undefined reference to/{s/.*`//;s/.$//p;q;}')
	[ -z "${undef}" ] && break

	debug 1 Trying to find ${undef}
	for attempt in ${ALLCOMP}; do
		debug 2 Component attempt: ${attempt}
		unset IFS
		nowundef=$(echo ${SEEDPROG}				\
		    | ${CC} -L${LIBDIR} ${CCPARAMS}			\
		      ${attempt} ${CURCOMP} ${RUMPBASE} 2>&1		\
		    | sed -n '/undefined reference to/{s/.*`//;s/.$//p;}')
		for one in ${nowundef}; do
			[ "${one}" = "${undef}" ] && continue 2
		done
		CURCOMP="${attempt} ${CURCOMP}"
		filter ${attempt} ALLCOMP
		debug 1 Found ${undef} from ${attempt}
		continue 2
	done
	die Internal error: unreachable statement
done

[ ! -z "${out}" ] && { echo 'ERROR:' >&2 ; echo ${out} >&2 ; exit 1 ; }
debug 0 Found a set
echo ${CURCOMP}
exit 0
