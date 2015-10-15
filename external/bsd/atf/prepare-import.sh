#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.8 2014/02/11 16:11:28 jmmv Exp $
#
# Use this script to recreate the 'dist' subdirectory from a newly released
# distfile.  The script takes care of unpacking the distfile, removing any
# files that are not relevant to NetBSD and checking if there are any new
# files in the new release that need to be addressed.
#

set -e

ProgName=${0##*/}

CLEAN_PATTERNS=
CLEAN_PATTERNS="${CLEAN_PATTERNS} *.m4"
CLEAN_PATTERNS="${CLEAN_PATTERNS} INSTALL TODO"
CLEAN_PATTERNS="${CLEAN_PATTERNS} Atffile */Atffile */*/Atffile"
CLEAN_PATTERNS="${CLEAN_PATTERNS} Makefile* */Makefile* */*/Makefile*"
CLEAN_PATTERNS="${CLEAN_PATTERNS} admin"
CLEAN_PATTERNS="${CLEAN_PATTERNS} atf-*/atf-*.m4"
CLEAN_PATTERNS="${CLEAN_PATTERNS} bconfig.h.in"
CLEAN_PATTERNS="${CLEAN_PATTERNS} bootstrap"
CLEAN_PATTERNS="${CLEAN_PATTERNS} configure*"
CLEAN_PATTERNS="${CLEAN_PATTERNS} m4"
CLEAN_PATTERNS="${CLEAN_PATTERNS} tools/generate-revision.sh"

err() {
	echo "${ProgName}:" "${@}" 1>&2
	exit 1
}

log() {
	echo "${ProgName}:" "${@}"
}

backup_dist() {
	if [ -d dist.old ]; then
		log "Removing dist; dist.old exists"
		rm -rf dist
	else
		log "Backing up dist as dist.old"
		mv dist dist.old
	fi
}

extract_distfile() {
	local distfile="${1}"; shift
	local distname="${1}"; shift

	log "Extracting ${distfile}"
	tar -xzf "${distfile}"
	[ -d "${distname}" ] || err "Distfile did not create ${distname}"
	log "Renaming ${distname} to dist"
	mv "${distname}" dist
}

get_distname() {
	local distfile="${1}"; shift
	basename "${distfile}" | sed -e 's,\.tar.*,,'
}

cleanup_dist() {
	log "Removing unnecessary files from dist"
	( cd dist && rm -rf ${CLEAN_PATTERNS} )
}

diff_dirs() {
	local old_dir="${1}"; shift
	local new_dir="${1}"; shift

	local tmpdir="$(mktemp -d -t atf-import)"
	trap "rm -rf '${tmpdir}'; exit 1" HUP INT QUIT TERM

	local old_list="${tmpdir}/old-list.txt"
	( cd "${old_dir}" && find . -type f | sort >>"${old_list}" )
	local new_list="${tmpdir}/new-list.txt"
	( cd "${new_dir}" && find . -type f | sort >>"${new_list}" )

	local added="${tmpdir}/added.txt"
	comm -13 "${old_list}" "${new_list}" >"${added}"
	local removed="${tmpdir}/removed.txt"
	comm -23 "${old_list}" "${new_list}" | grep -v '/CVS' >"${removed}"
	if [ -s "${removed}" ]; then
		log "Removed files found"
		cat "${removed}"
	fi
	if [ -s "${added}" ]; then
		log "New files found"
		cat "${added}"
		log "Check if any files have to be cleaned up and update" \
		    "the prepare-import.sh script accordingly"
	fi

	rm -rf "${tmpdir}"
}

main() {
	[ ${#} -eq 1 ] || err "Must provide a distfile name"
	local distfile="${1}"; shift

	[ -f Makefile -a -f prepare-import.sh ] || \
	    err "Must be run from the src/external/bsd/atf subdirectory"

	local distname="$(get_distname ${distfile})"

	backup_dist
	extract_distfile "${distfile}" "${distname}"
	cleanup_dist
	diff_dirs dist.old dist
}

main "${@}"
