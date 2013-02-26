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


. "${KYUA_ATF_COMPAT_PKGDATADIR:-__PKGDATADIR__}/tests_lib.subr"


atf_test_case all_pass
all_pass_body() {
    create_test_program program1 pass skip
    create_atffile Atffile 'prop: test-suite = "suite"' \
        'tp: program1' 'tp: subdir'

    mkdir subdir
    create_test_program subdir/program2 skip
    create_atffile subdir/Atffile 'prop: test-suite = "suite"' \
        'tp: program2'

    atf_check -s exit:0 -e ignore \
        -o match:'program1:pass  ->  passed' \
        -o match:'program1:skip  ->  skipped' \
        -o match:'subdir/program2:skip  ->  skipped' \
        -o match:'Committed action 1' \
        atf-run
}


atf_test_case some_fail
some_fail_body() {
    create_test_program program1 pass skip
    create_atffile Atffile 'prop: test-suite = "suite"' \
        'tp: program1' 'tp: subdir'

    mkdir subdir
    create_atffile subdir/Atffile 'tp-glob: *'

    mkdir subdir/nested
    create_test_program subdir/nested/program2 skip fail
    create_atffile subdir/nested/Atffile 'prop: test-suite = "suite"' \
        'tp-glob: program2*'

    atf_check -s exit:1 -e ignore \
        -o match:'program1:pass  ->  passed' \
        -o match:'program1:skip  ->  skipped' \
        -o match:'subdir/nested/program2:fail  ->  failed' \
        -o match:'subdir/nested/program2:skip  ->  skipped' \
        -o match:'Committed action 1' \
        atf-run
}

atf_test_case prefer_kyuafiles
prefer_kyuafiles_body() {
    create_test_program program1 pass skip
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
test_suite('foo')
atf_test_program{name='program1'}
EOF
    echo "This file is invalid" >Atffile

    atf_check -s exit:0 -e ignore \
        -o match:'program1:pass  ->  passed' \
        -o match:'program1:skip  ->  skipped' \
        atf-run
}


atf_test_case selectors
selectors_body() {
    create_test_program program1 pass skip
    create_atffile Atffile 'prop: test-suite = "suite"' \
        'tp: program1' 'tp: subdir'

    mkdir subdir
    create_atffile subdir/Atffile 'tp-glob: *'

    mkdir subdir/nested
    create_test_program subdir/nested/program2 skip fail
    create_atffile subdir/nested/Atffile 'prop: test-suite = "suite"' \
        'tp-glob: program2*'

    atf_check -s exit:1 -e ignore \
        -o match:'program1:pass  ->  passed' \
        -o match:'program1:skip  ->  skipped' \
        -o match:'subdir/nested/program2:fail  ->  failed' \
        -o match:'subdir/nested/program2:skip  ->  skipped' \
        -o match:'Committed action 1' \
        atf-run

    atf_check -s exit:0 -e ignore \
        -o match:'program1:pass  ->  passed' \
        -o not-match:'program1:skip  ->  skipped' \
        -o not-match:'subdir/nested/program2:fail  ->  failed' \
        -o match:'subdir/nested/program2:skip  ->  skipped' \
        -o match:'Committed action 2' \
        atf-run program1:pass subdir/nested/program2:skip

    atf_check -s exit:1 -e ignore \
        -o not-match:'program1:pass  ->  passed' \
        -o not-match:'program1:skip  ->  skipped' \
        -o match:'subdir/nested/program2:fail  ->  failed' \
        -o match:'subdir/nested/program2:skip  ->  skipped' \
        -o match:'Committed action 3' \
        atf-run subdir/nested/program2
}


atf_test_case config__priorities
config__priorities_body()
{
    mkdir system
    export ATF_CONFDIR="$(pwd)/system"
    mkdir user
    mkdir user/.atf
    export HOME="$(pwd)/user"

    create_test_program helper config
    create_atffile Atffile 'prop: test-suite = "irrelevant"' 'tp: helper'

    echo "Checking system-wide configuration only"
    create_config system/common.conf '   unprivileged-user  =    "nobody"'
    atf_check -s exit:0 -o 'match:helper:config  ->  passed' -e ignore atf-run
    atf_check -s exit:0 -o 'inline:unprivileged-user = nobody\n' \
        cat config.out

    echo "Checking user-specific overrides"
    create_config user/.atf/common.conf '	unprivileged-user =   "root"'
    atf_check -s exit:0 -o 'match:helper:config  ->  passed' -e ignore atf-run
    atf_check -s exit:0 -o 'inline:unprivileged-user = root\n' \
        cat config.out

    echo "Checking command-line overrides"
    atf_check -s exit:0 -o 'match:helper:config  ->  passed' -e ignore atf-run \
        -v"unprivileged-user=$(id -u -n)"
    atf_check -s exit:0 -o "inline:unprivileged-user = $(id -u -n)\n" \
        cat config.out
}


atf_test_case config__test_suites__files
config__test_suites__files_body()
{
    mkdir system
    export ATF_CONFDIR="$(pwd)/system"

    create_config system/suite1.conf 'var1 = "var1-for-suite1"' \
        'var2 = "var2-for-suite1"'
    create_config system/suite2.conf 'var1 = "var1-for-suite2"' \
        'var2 = "var2-for-suite2"'

    create_test_program helper1 config
    create_atffile Atffile 'prop: test-suite = "suite1"' 'tp: helper1' \
        'tp: subdir'
    mkdir subdir
    create_test_program subdir/helper2 config
    create_atffile subdir/Atffile 'prop: test-suite = "suite2"' 'tp: helper2'

    atf_check -s exit:0 -o ignore -e ignore atf-run helper1
    atf_check -s exit:0 -o 'match:var1 = var1-for-suite1' \
        -o 'match:var2 = var2-for-suite1' -o 'not-match:suite2' \
        cat config.out

    atf_check -s exit:0 -o ignore -e ignore atf-run subdir/helper2
    atf_check -s exit:0 -o 'match:var1 = var1-for-suite2' \
        -o 'match:var2 = var2-for-suite2' -o 'not-match:suite1' \
        cat config.out
}


atf_test_case config__test_suites__vflags
config__test_suites__vflags_body()
{
    create_test_program helper1 config
    create_atffile Atffile 'prop: test-suite = "suite1"' 'tp: helper1' \
        'tp: subdir'
    mkdir subdir
    create_test_program subdir/helper2 config
    create_atffile subdir/Atffile 'prop: test-suite = "suite2"' 'tp: helper2'

    atf_check -s exit:0 -o ignore -e ignore atf-run -v var1=foo helper1
    atf_check -s exit:0 -o 'match:var1 = foo' cat config.out

    atf_check -s exit:0 -o ignore -e ignore atf-run -v var1=bar helper1
    atf_check -s exit:0 -o 'match:var1 = bar' cat config.out
}


atf_test_case unknown_option
unknown_option_body() {
    atf_check -s exit:1 -o empty -e match:'E: Unknown option -Z' \
        atf-run -Z -A
}


atf_init_test_cases() {
    atf_add_test_case all_pass
    atf_add_test_case some_fail
    atf_add_test_case selectors

    atf_add_test_case prefer_kyuafiles

    atf_add_test_case config__priorities
    atf_add_test_case config__test_suites__files
    atf_add_test_case config__test_suites__vflags

    atf_add_test_case unknown_option
}
