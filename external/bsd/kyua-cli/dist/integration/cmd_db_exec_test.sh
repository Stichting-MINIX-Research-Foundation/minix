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


utils_test_case one_arg
one_arg_body() {
    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec "SELECT * FROM metadata"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case many_args
many_args_body() {
    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec SELECT "*" FROM metadata
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case no_args
no_args_body() {
    atf_check -s exit:3 -o empty -e match:"Not enough arguments" kyua db-exec
    test ! -f .kyua/store.db || atf_fail "Database created but it should" \
        "not have been"
}


utils_test_case invalid_statement
invalid_statement_body() {
    atf_check -s exit:1 -o empty -e match:"SQLite error.*foo" \
        kyua db-exec foo
    test -f .kyua/store.db || atf_fail "Database not created as part of" \
        "initialization"
}


utils_test_case store_flag__default_home
store_flag__default_home_body() {
    HOME=home-dir
    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec "SELECT * FROM metadata"
    test -f home-dir/.kyua/store.db || atf_fail "Database not created in" \
        "the home directory"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case store_flag__explicit__ok
store_flag__explicit__ok_body() {
    HOME=home-dir
    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua --logfile=/dev/null db-exec -s store.db "SELECT * FROM metadata"
    test ! -d home-dir/.kyua || atf_fail "Home directory created but this" \
        "should not have happened"
    test -f store.db || atf_fail "Database not created in expected directory"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case store_flag__explicit__fail
store_flag__explicit__fail_head() {
    atf_set "require.user" "unprivileged"
}
store_flag__explicit__fail_body() {
    mkdir dir
    chmod 555 dir
    atf_check -s exit:2 -o empty -e match:"Cannot open.*dir/foo.db" \
        kyua db-exec --store=dir/foo.db "SELECT * FROM metadata"
}


utils_test_case no_headers_flag
no_headers_flag_body() {
    atf_check kyua db-exec "CREATE TABLE data" \
        "(a INTEGER PRIMARY KEY, b INTEGER, c TEXT)"
    atf_check kyua db-exec "INSERT INTO data VALUES (65, 43, NULL)"
    atf_check kyua db-exec "INSERT INTO data VALUES (23, 42, 'foo')"

    cat >expout <<EOF
a,b,c
23,42,foo
65,43,NULL
EOF
    atf_check -s exit:0 -o file:expout -e empty \
        kyua db-exec "SELECT * FROM data ORDER BY a"

    tail -n 2 <expout >expout2
    atf_check -s exit:0 -o file:expout2 -e empty \
        kyua db-exec --no-headers "SELECT * FROM data ORDER BY a"
}


atf_init_test_cases() {
    atf_add_test_case one_arg
    atf_add_test_case many_args
    atf_add_test_case no_args
    atf_add_test_case invalid_statement

    atf_add_test_case store_flag__default_home
    atf_add_test_case store_flag__explicit__ok
    atf_add_test_case store_flag__explicit__fail

    atf_add_test_case no_headers_flag
}
