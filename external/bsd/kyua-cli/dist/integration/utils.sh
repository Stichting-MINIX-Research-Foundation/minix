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


# Subcommand to strip the timestamps of a report.
#
# This is to make the reports deterministic and thus easily testable.  The
# timestamps are replaced by the fixed string S.UUUs.
#
# This variable should be used as shown here:
#
#     atf_check ... -x kyua report "| ${uilts_strip_timestamp}"
utils_strip_timestamp='sed -e "s,[0-9][0-9]*.[0-9][0-9][0-9]s,S.UUUs,g"'


# Copies a helper binary from the source directory to the work directory.
#
# \param name The name of the binary to copy.
# \param destination The target location for the binary; can be either
#     a directory name or a file name.
utils_cp_helper() {
    local name="${1}"; shift
    local destination="${1}"; shift

    ln -s "$(atf_get_srcdir)"/helpers/"${name}" "${destination}"
}


# Creates a 'kyua' binary in the path that strips timestamps off the output.
#
# Call this on test cases that wish to replace timestamps in the *stdout* of
# Kyua with the S.UUUs deterministic string.  This is usable for tests that
# validate the 'test' subcommand, but also by a few specific tests for the
# 'report' subcommand.
utils_install_timestamp_wrapper() {
    [ ! -x kyua ] || return
    cat >kyua <<EOF
#! /bin/sh

PATH=${PATH}

kyua "\${@}" >kyua.tmpout
result=\${?}
cat kyua.tmpout | ${utils_strip_timestamp}
exit \${result}
EOF
    chmod +x kyua
    PATH="$(pwd):${PATH}"
}


# Defines a test case with a default head.
utils_test_case() {
    local name="${1}"; shift

    atf_test_case "${name}"
    eval "${name}_head() {
        atf_set require.progs kyua
    }"
}
