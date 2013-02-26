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

# \file atf-report.sh
# Kyua-based compatibility replacement for atf-report.


. "${KYUA_ATF_COMPAT_PKGDATADIR:-__PKGDATADIR__}/lib.subr"


# Gets the action identifier from the output of 'kyua test'.
#
# \param file The file that contains the output of 'kyua test'.  Can be
#     /dev/stdout.
#
# \post Prints the action identifier.
get_action() {
    local file="${1}"; shift
    grep '^Committed action ' "${file}" | cut -d ' ' -f 3
}


# Generates an HTML report.
#
# The original atf-report generates HTML reports that are made up of solely a
# single HTML page.  Because of this, such reports can be written directly to
# the file specified by the user.
#
# Because Kyua generates "rich" HTML reports (i.e. reports that consist of more
# than one HTML page), we cannot perfectly emulate atf-report.  Instead, we
# create an auxiliary directory to hold all the files, and then place a link to
# such files in the file specified by the user.  The drawback is that HTML
# reports sent to stdout are no longer possible.
#
# \param output_file The name of the file to which to write the HTML report.
#     This file will end up being a symlink to the real report.
report_html() {
    local output_file="${1}"; shift

    [ "${output_file}" != "/dev/stdout" ] || \
        lib_usage_error "Cannot write HTML reports to stdout"

    local dir="$(dirname "${output_file}")"
    local index_name="${output_file##*/}"
    local files_name="$(echo "${index_name}" | sed -e 's,\.[a-zA-Z]*$,,').files"

    kyua report-html --action="$(get_action /dev/stdin)" \
        --output="${dir}/${files_name}"

    echo "Pointing ${index_name} to ${files_name}/index.html"
    ( cd "${dir}" && ln -s "${files_name}/index.html" "${index_name}" )
}


# Genereates an XML report.
#
# For our compatibility purposes, we assume that the XML report is just an HTML
# report.
#
# \param output_file The name of the file to which to write the HTML report.
#     This file will end up being a symlink to the real report.
report_xml() {
    local output_file="${1}"; shift

    lib_warning "XML output not supported; generating HTML instead"
    report_html "${output_file}"
}


# Generates progressive textual reports.
#
# This wrapper attempts to emulate atf-report's ticker output by reading the
# output of 'kyua test' progressively and sending it to the screen as soon as it
# becomes available.  The tail of the 'kyua test' report that includes summaries
# for the run is suppressed and is replaced with the more-detailed output of
# 'kyua report'.
#
# \param output_file The name of the file to which to write the textual report.
#     Can be /dev/stdout.
report_ticker() {
    local output_file="${1}"; shift

    local print=yes
    while read line; do
        [ -n "${line}" ] || print=no

        if [ "${print}" = yes ]; then
            case "${line}" in
            Committed*)
                echo "${line}" >>"${Lib_TempDir}/output"
                ;;
            *)
                echo "${line}"
                ;;
            esac
        else
            echo "${line}" >>"${Lib_TempDir}/output"
        fi
    done

    kyua report --action="$(get_action "${Lib_TempDir}/output")" \
        --output="${output_file}"
}


# Generates a report based on an output specification.
#
# \param output_spec The format:file specification of the output.
report() {
    local output_spec="${1}"; shift

    local output_format="$(echo "${output_spec}" | cut -d : -f 1)"
    local output_file="$(echo "${output_spec}" | cut -d : -f 2)"
    [ "${output_file}" != - ] || output_file=/dev/stdout

    case "${output_format}" in
    html|ticker|xml)
        "report_${output_format}" "${output_file}"
        ;;

    *)
        lib_usage_error "Unknown output format '${output_format}'"
        ;;
    esac
}


# Prints program usage to stdout.
#
# \param progname The name of the program to use for the syntax help.
usage() {
    local progname="${1}"; shift
    echo "Usage: ${progname} [-o output-spec]"
}


# Entry point for the program.
#
# \param ... The user-provided arguments.
main()
{
    local output_spec="ticker:-"
    while getopts ':o:' arg "${@}"; do
        case "${arg}" in
        o)
            output_spec="${OPTARG}"
            ;;
        \?)
            lib_usage_error "Unknown option -${OPTARG}"
            ;;
        esac
    done
    shift $((${OPTIND} - 1))

    [ ${#} -eq 0 ] || lib_usage_error "No arguments allowed"

    lib_init_tempdir

    report "${output_spec}"

    lib_cleanup
}


main "${@}"
