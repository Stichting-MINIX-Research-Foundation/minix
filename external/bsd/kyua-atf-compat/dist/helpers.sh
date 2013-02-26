#! __ATF_SH__
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


# Path outside of this test program's work directory to which we can write
# temporary files for inspection by the caller.
CONTROL_DIR='@CONTROL_DIR@'


# This is set by the test programs when copying the helpers into the work
# directory to specify which test cases to enable.
ENABLED_TESTS='@ENABLED_TESTS@'


atf_test_case fail
fail_body() {
    atf_fail "On purpose"
}


atf_test_case pass
pass_body() {
    :
}


atf_test_case skip
skip_body() {
    atf_skip "Skipped reason"
}


atf_test_case config
config_body() {
    local outfile="${CONTROL_DIR}/config.out"

    rm "${outfile}"
    touch "${outfile}"
    for var in unprivileged-user var1 var2 var3; do
        if atf_config_has "${var}"; then
            echo "${var} = $(atf_config_get "${var}")" >>"${outfile}"
        fi
    done
}


atf_init_test_cases() {
    for test_name in ${ENABLED_TESTS}; do
        atf_add_test_case "${test_name}"
    done
}
