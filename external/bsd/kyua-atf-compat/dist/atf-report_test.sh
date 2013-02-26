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


atf_test_case ticker__no_tests
ticker__no_tests_body() {
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
EOF

    atf_check -s exit:0 -o save:run.log -e ignore atf-run

    cat >expout <<EOF
===> Summary
Action: 1
Test cases: 0 total, 0 skipped, 0 expected failures, 0 broken, 0 failed
Total time: X.XXXs
EOF
    atf_check -s exit:0 -o save:report.log -e empty atf-report <run.log
    strip_timestamps report.log
    atf_check -s exit:0 -o file:expout cat report.log
}


atf_test_case ticker__some_tests
ticker__some_tests_body() {
    create_test_program program1 skip
    create_test_program program2 fail skip pass
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
test_suite('foo')
atf_test_program{name='program1'}
atf_test_program{name='program2'}
EOF
    atf_check -s exit:1 -o save:run.log -e ignore atf-run

    cat >expout <<EOF
program1:skip  ->  skipped: Skipped reason  [X.XXXs]
program2:fail  ->  failed: On purpose  [X.XXXs]
program2:skip  ->  skipped: Skipped reason  [X.XXXs]
program2:pass  ->  passed  [X.XXXs]
===> Skipped tests
program1:skip  ->  skipped: Skipped reason  [X.XXXs]
program2:skip  ->  skipped: Skipped reason  [X.XXXs]
===> Failed tests
program2:fail  ->  failed: On purpose  [X.XXXs]
===> Summary
Action: 1
Test cases: 4 total, 2 skipped, 0 expected failures, 0 broken, 1 failed
Total time: X.XXXs
EOF
    atf_check -s exit:0 -o save:report.log -e empty atf-report <run.log
    strip_timestamps report.log
    atf_check -s exit:0 -o file:expout cat report.log
}


atf_test_case ticker__explicit
ticker__explicit_body() {
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
EOF

    atf_check -s exit:0 -o save:run.log -e ignore atf-run

    cat >expout <<EOF
===> Summary
Action: 1
Test cases: 0 total, 0 skipped, 0 expected failures, 0 broken, 0 failed
Total time: X.XXXs
EOF
    atf_check -s exit:0 -o empty -e empty \
        atf-report -o ticker:my-report.log <run.log
    strip_timestamps my-report.log
    atf_check -s exit:0 -o file:expout cat my-report.log
}


atf_test_case html__no_tests
html__no_tests_body() {
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
EOF
    atf_check -s exit:0 -o save:run.log -e ignore atf-run
    atf_check -s exit:0 -o ignore -e empty \
        atf-report -o html:report.html <run.log

    echo "All generated files"
    ls -l report.html report.files

    for file in index.html context.html report.css; do
        test -f report.files/"${file}" || \
            atf_fail "Expected file ${file} not found"
    done

    test report.html -ef report.files/index.html || \
        atf_fail "Index file link not created properly"
}


atf_test_case html__some_tests
html__some_tests_body() {
    create_test_program program1 skip
    create_test_program program2 fail skip pass
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
test_suite('foo')
atf_test_program{name='program1'}
atf_test_program{name='program2'}
EOF
    atf_check -s exit:1 -o save:run.log -e ignore atf-run
    atf_check -s exit:0 -o ignore -e empty \
        atf-report -o html:report.html <run.log

    echo "All generated files"
    ls -l report.html report.files

    for file in index.html context.html report.css \
        program1_skip.html program2_fail.html  program2_skip.html \
        program2_pass.html
    do
        test -f report.files/"${file}" || \
            atf_fail "Expected file ${file} not found"
    done

    test report.html -ef report.files/index.html || \
        atf_fail "Index file link not created properly"
}


atf_test_case html__other_directory
html__other_directory_body() {
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
EOF
    atf_check -s exit:0 -o save:run.log -e ignore atf-run
    mkdir -p a/b/c
    atf_check -s exit:0 -o ignore -e empty \
        atf-report -o html:$(pwd)/a/b/c/report.html <run.log

    echo "All generated files"
    ls -l a/b/c/*

    for file in index.html context.html report.css; do
        test -f a/b/c/report.files/"${file}" || \
            atf_fail "Expected file ${file} not found"
    done

    test a/b/c/report.html -ef a/b/c/report.files/index.html || \
        atf_fail "Index file link not created properly"
}


atf_test_case html__to_stdout
html__to_stdout_body() {
    atf_check -s exit:1 -o empty \
        -e match:'E: Cannot write HTML reports to stdout' \
        atf-report -o html:-
    atf_check -s exit:1 -o empty \
        -e match:'E: Cannot write HTML reports to stdout' \
        atf-report -o html:/dev/stdout
}


atf_test_case xml_is_html
xml_is_html_body() {
    cat >Kyuafile <<EOF
syntax('kyuafile', 1)
EOF
    atf_check -s exit:0 -o save:run.log -e ignore atf-run
    atf_check -s exit:0 -o ignore \
        -e match:'XML.*not supported.*generating HTML' \
        atf-report -o xml:report.html <run.log

    for file in index.html context.html report.css; do
        test -f report.files/"${file}" || \
            atf_fail "Expected file ${file} not found"
    done

    test report.html -ef report.files/index.html || \
        atf_fail "Index file link not created properly"
}


atf_test_case unknown_option
unknown_option_body() {
    atf_check -s exit:1 -o empty -e match:'E: Unknown option -Z' \
        atf-report -Z -A
}


atf_test_case too_many_arguments
too_many_arguments_body() {
    atf_check -s exit:1 -o empty -e match:'E: No arguments allowed' \
        atf-report first
}


atf_test_case unknown_format
unknown_format_body() {
    atf_check -s exit:1 -o empty -e match:"E: Unknown output format 'csv'" \
        atf-report -o csv:file.csv
}


atf_init_test_cases() {
    atf_add_test_case ticker__no_tests
    atf_add_test_case ticker__some_tests
    atf_add_test_case ticker__explicit

    atf_add_test_case html__no_tests
    atf_add_test_case html__some_tests
    atf_add_test_case html__other_directory
    atf_add_test_case html__to_stdout

    atf_add_test_case xml_is_html

    atf_add_test_case unknown_option
    atf_add_test_case too_many_arguments
    atf_add_test_case unknown_format
}
