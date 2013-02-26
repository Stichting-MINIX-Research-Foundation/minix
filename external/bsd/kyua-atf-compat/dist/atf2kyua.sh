#! __SH__
# Copyright 2011 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# \file atf2kyua.sh
# Converts Atffiles to Kyuafiles for a particular test suite.


. "${KYUA_ATF_COMPAT_PKGDATADIR:-__PKGDATADIR__}/lib.subr"


# Prunes all Kyuafiles from a test suite in preparation for regeneration.
#
# \param target_root The path to the test suite.
remove_kyuafiles() {
    local target_root="${1}"; shift

    if [ -d "${target_root}" ]; then
        lib_info "Removing stale Kyuafiles from ${target_root}"
        find "${target_root}" -name Kyuafile -exec rm -f {} \;
    fi
}


# Obtains the list of test programs and subdirectories referenced by an Atffile.
#
# Any globs within the Atffile are expanded relative to the directory in which
# the Atffile lives.
#
# \param atffile The path to the Atffile to process.
#
# \post Prints the list of files referenced by the Atffile on stdout.
extract_files() {
    local atffile="${1}"; shift

    local dir="$(dirname "${atffile}")"

    local globs="$(grep '^tp-glob:' "${atffile}" | cut -d ' ' -f 2-)"
    local files="$(grep '^tp:' "${atffile}" | cut -d ' ' -f 2-)"

    for file in ${files} $(cd "$(dirname "${atffile}")" && echo ${globs}); do
        if test -d "${dir}/${file}" -o -x "${dir}/${file}"; then
            echo "${file}"
        fi
    done
}


# Converts an Atffile to a Kyuafile.
#
# \param atffile The path to the Atfffile to convert.
# \param kyuafile The path to where the Kyuafile will be written.
convert_atffile() {
    local atffile="${1}"; shift
    local kyuafile="${1}"; shift

    lib_info "Converting ${atffile} -> ${kyuafile}"

    local test_suite="$(grep 'prop:.*test-suite.*' "${atffile}" \
        | cut -d \" -f 2)"

    local dir="$(dirname "${atffile}")"

    local subdirs=
    local test_programs=
    for file in $(extract_files "${atffile}"); do
        if test -f "${dir}/${file}/Atffile"; then
            subdirs="${subdirs} ${file}"
        elif test -x "${dir}/${file}"; then
            test_programs="${test_programs} ${file}"
        fi
    done

    mkdir -p "$(dirname "${kyuafile}")"

    echo "syntax('kyuafile', 1)" >"${kyuafile}"
    echo >>"${kyuafile}"
    echo "test_suite('${test_suite}')" >>"${kyuafile}"
    if [ -n "${subdirs}" ]; then
        echo >>"${kyuafile}"
        for dir in ${subdirs}; do
            echo "include('${dir}/Kyuafile')" >>"${kyuafile}"
        done
    fi
    if [ -n "${test_programs}" ]; then
        echo >>"${kyuafile}"
        for tp in ${test_programs}; do
            echo "atf_test_program{name='${tp}'}" >>"${kyuafile}"
        done
    fi
}


# Adds Kyuafiles to a test suite by converting any existing Atffiles.
#
# \param source_root The path to the existing test suite root.  Must contain
#     an Atffile and the test programs.
# \param target_root The path to the directory where the Kyuafiles will be
#     written.  The layout will mimic that of source_root.
add_kyuafiles() {
    local source_root="${1}"; shift
    local target_root="${1}"; shift

    for atffile in $(cd "${source_root}" && find . -name Atffile); do
        local subdir="$(echo "${atffile}" | sed 's,Atffile$,,;s,^\./,,')"
        convert_atffile "${source_root}/${subdir}Atffile" \
            "${target_root}/${subdir}Kyuafile"
    done
}


# Prints program usage to stdout.
#
# \param progname The name of the program to use for the syntax help.
usage() {
    local progname="${1}"; shift
    echo "Usage: ${progname} [-s source_root] [-t target_root]"
}


# Entry point for the program.
#
# \param ... The user-provided arguments.
main() {
    local source_root=
    local target_root=

    while getopts ':s:t:' arg "${@}"; do
        case "${arg}" in
        s)
            source_root="${OPTARG}"
            ;;
        t)
            target_root="${OPTARG}"
            ;;
        \?)
            lib_usage_error "Unknown option -${OPTARG}"
            ;;
        esac
    done
    shift $((${OPTIND} - 1))

    [ -n "${source_root}" ] || source_root=.
    [ -n "${target_root}" ] || target_root="${source_root}"

    [ ${#} -eq 0 ] || lib_usage_error "No arguments allowed"

    [ -f "${source_root}/Atffile" ] || \
        lib_error "${source_root} is not a test suite; missing Atffile"

    remove_kyuafiles "${target_root}"
    add_kyuafiles "${source_root}" "${target_root}"
}


main "${@}"
