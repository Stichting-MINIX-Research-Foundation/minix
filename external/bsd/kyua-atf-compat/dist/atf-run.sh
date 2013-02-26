#! __SH__
# Copyright 2012 Google Inc.
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

# \file atf-run.sh
# Kyua-based compatibility replacement for atf-run.


. "${KYUA_ATF_COMPAT_PKGDATADIR:-__PKGDATADIR__}/lib.subr"


# Path to ATF's configuration directory.
ATF_CONFDIR="${ATF_CONFDIR:-__ATF_CONFDIR__}"


# Path to the bin directory where we got installed.
BINDIR="${KYUA_ATF_COMPAT_BINDIR:-__BINDIR__}"


# Loads configuration variables from a set of files.
#
# \param dir The directory from which to load the configuration files.
# \param output_var The name of the variable that will accumulate all the
#     --variable flags to represent the configuration file.
load_configs() {
    local dir="${1}"; shift
    local output_var="${1}"; shift

    local all_vars=
    for file in "${dir}"/*; do
        [ "${file}" != "${dir}/*" ] || break

        lib_info "Loading configuration from ${file}"

        local prefix
        case "${file}" in
        *common.conf) prefix= ;;
        *) prefix="test_suites.$(basename "${file}" | sed -e 's,.conf$,,')." ;;
        esac

        local ws='[ 	]*'  # That's a space and a tab.
        local name='[a-zA-Z][-_a-zA-Z0-9]*'
        local repl="--variable='${prefix}\\1=\\2'"
        local vars="$(grep "^${ws}${name}${ws}=" "${file}" | \
            sed -e 's,#(.*)$,,;s,unprivileged-user,unprivileged_user,g' \
            -e "s,^${ws}\(${name}\)${ws}=${ws}'\([^']*\)'${ws}$,${repl}," \
            -e "s,^${ws}\(${name}\)${ws}=${ws}\"\([^\"]*\)\"${ws}$,${repl}," \
            -e "s,^${ws}\(${name}\)${ws}=${ws}\(.*\)$,${repl},")"

        lib_info "Extracted arguments: ${vars}"
        all_vars="${all_vars} ${vars}"
    done
    eval ${output_var}=\'"${all_vars}"\'
}


# Transforms an atf-run -v specification to Kyua's semantics.
#
# \param raw The key=value argument to -v.
# \param ... The names of all the possible test suites.  For test-suite specific
#     variables, we expand them to all their possibilities as Kyua needs to know
#     what test suite a particular variable belongs to.
#
# \post Prints one or more --variable arguments that match the input variable
# name.
convert_variable() {
    local raw="${1}"; shift

    case "${raw}" in
    unprivileged-user=*)
        echo "--variable=${raw}" | \
            sed -e 's,unprivileged-user,unprivileged_user,g'
        ;;
    *)
        for test_suite in "${@}"; do
            echo "--variable=test_suites.${test_suite}.${raw}"
        done
        ;;
    esac
}


# Collects all the test suite names defined in a subtree.
#
# \param dir The subtree to scan for Kyuafiles.
#
# \post Prints the names of all found test suites.
grab_test_suites() {
    local dir="${1}"; shift

    find "${dir}" -name Kyuafile -exec grep "test_suite('" "{}" \; | \
        cut -d "'" -f 2
}


# Gets the path to the compatibility Kyuafile.
#
# If a Kyuafile is found in the current directory, use that directly.
# Otherwise, generate a fake Kyuafile in a temporary directory and return
# that instead.
#
# \param [out] output_var The name of the variable to set with the path
#     of the Kyuafile to be used.
select_kyuafile() {
    local output_var="${1}"; shift

    if [ -f Kyuafile ]; then
        eval ${output_var}=\'"$(pwd)/Kyuafile"\'
    elif [ -f Atffile ]; then
        "${BINDIR}/atf2kyua" -s "$(pwd)" -t "${Lib_TempDir}" || \
            lib_error "Cannot generate fake Kyuafile"
        eval ${output_var}=\'"${Lib_TempDir}/Kyuafile"\'
    else
        lib_error "Cannot find Atffile nor Kyuafile"
    fi
}


# Prints program usage to stdout.
#
# \param progname The name of the program to use for the syntax help.
usage() {
    local progname="${1}"; shift
    echo "Usage: ${progname} [-v var-value] [program1 [.. programN]]"
}


# Entry point for the program.
#
# \param ... The user-provided arguments.
main() {
    local vflags=

    while getopts ':v:' arg "${@}"; do
        case "${arg}" in
        v)
            vflags="${OPTARG} ${cli_variables}"
            ;;
        \?)
            lib_usage_error "Unknown option -${OPTARG}"
            ;;
        esac
    done
    shift $((${OPTIND} - 1))

    lib_init_tempdir

    load_configs "${ATF_CONFDIR}" "system_variables"  # Sets system_variables.
    load_configs "${HOME}/.atf" "user_variables"  # Sets user_variables.

    local kyuafile_path
    select_kyuafile "kyuafile_path"  # Sets kyuafile_path.

    local test_suites="$(grab_test_suites "$(dirname "${kyuafile_path}")")"

    cli_variables=
    for vflag in ${vflags}; do
        local values="$(convert_variable "${vflag}" ${test_suites})"
        cli_variables="${values} ${cli_variables}"
    done

    kyua ${system_variables} ${user_variables} ${cli_variables} \
        test --kyuafile="${kyuafile_path}" --build-root="$(pwd)" "${@}"
    local ret="${?}"

    lib_cleanup

    return "${ret}"
}


main "${@}"
