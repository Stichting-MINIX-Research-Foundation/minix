# Copyright 2013 Google Inc.
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


utils_test_case upgrade
upgrade_head() {
    data=$(atf_get_srcdir)/../store

    atf_set require.files "${data}/schema_v1.sql ${data}/testdata_v1.sql"
    atf_set require.progs "sqlite3"
}
upgrade_body() {
    data=$(atf_get_srcdir)/../store

    mkdir .kyua
    cat "${data}/schema_v1.sql" "${data}/testdata_v1.sql" \
        | sqlite3 .kyua/store.db
    atf_check -s exit:0 -o empty -e empty kyua db-migrate
}


utils_test_case already_up_to_date
already_up_to_date_body() {
    atf_check -s exit:0 -o ignore -e empty \
        kyua db-exec "SELECT * FROM metadata"  # Create database.
    atf_check -s exit:1 -o empty -e match:"already at schema version" \
        kyua db-migrate
}


utils_test_case need_upgrade
need_upgrade_head() {
    data=$(atf_get_srcdir)/../store

    atf_set require.files "${data}/schema_v1.sql"
    atf_set require.progs "sqlite3"
}
need_upgrade_body() {
    data=$(atf_get_srcdir)/../store

    mkdir .kyua
    sqlite3 .kyua/store.db <"${data}/schema_v1.sql"
    atf_check -s exit:2 -o empty \
        -e match:"database has schema version 1.*use db-migrate" kyua report
}


utils_test_case store_flag__ok
store_flag__ok_body() {
    echo "This is not a valid database" >test.db
    atf_check -s exit:1 -o empty -e match:"Migration failed" \
        kyua db-migrate --store ./test.db
}


utils_test_case store_flag__fail
store_flag__fail_body() {
    atf_check -s exit:1 -o empty -e match:"Cannot open.*test.db" \
        kyua db-migrate --store ./test.db
}


utils_test_case too_many_arguments
too_many_arguments_body() {
    cat >stderr <<EOF
Usage error for command db-migrate: Too many arguments.
Type 'kyua help db-migrate' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:stderr kyua db-migrate abc def
}


atf_init_test_cases() {
    atf_add_test_case upgrade
    atf_add_test_case already_up_to_date
    atf_add_test_case need_upgrade

    atf_add_test_case store_flag__ok
    atf_add_test_case store_flag__fail

    atf_add_test_case too_many_arguments
}
