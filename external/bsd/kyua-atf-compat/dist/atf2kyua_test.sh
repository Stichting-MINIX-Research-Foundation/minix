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


# Performs a conversion test of an Atffile without recursion.
#
# \param source_root The directory in which to create the fake test suite.  Must
#     exist.
# \param target_root The directory in which the generated Kyuafiles will be
#     stored.  May not exist.
# \param ... Additional parameters to pass to atf2kyua.
one_level_test() {
    local source_root="${1}"; shift
    local target_root="${1}"; shift

    cd "${source_root}"
    create_test_program p1 pass
    create_test_program p2 pass
    create_atffile "Atffile" 'prop: test-suite = "foo"' 'tp: p1' 'tp: p2'
    cd -

    atf_check -s exit:0 -e ignore atf2kyua "${@}"

    cd "${target_root}"
    cat >expected <<EOF
syntax('kyuafile', 1)

test_suite('foo')

atf_test_program{name='p1'}
atf_test_program{name='p2'}
EOF
    atf_check -s exit:0 -o file:expected cat "Kyuafile"
    cd -
}


# Performs a conversion test of an Atffile with recursion.
#
# \param source_root The directory in which to create the fake test suite.  Must
#     exist.
# \param target_root The directory in which the generated Kyuafiles will be
#     stored.  May not exist.
# \param ... Additional parameters to pass to atf2kyua.
multiple_levels_test() {
    local source_root="${1}"; shift
    local target_root="${1}"; shift

    cd "${source_root}"
    create_test_program prog1 pass

    mkdir dir1
    create_test_program dir1/ignore-me pass
    create_test_program dir1/prog2 fail

    mkdir dir1/dir2
    create_test_program dir1/dir2/prog3 skip
    create_test_program dir1/dir2/prog4 pass

    mkdir dir3
    create_test_program dir3/prog1 fail

    create_atffile Atffile 'prop: test-suite = "foo"' 'tp: prog1' 'tp: dir1'
    create_atffile dir1/Atffile 'prop: test-suite = "bar"' 'tp-glob: [pd]*'
    create_atffile dir1/dir2/Atffile 'prop: test-suite = "foo"' 'tp: prog3' \
        'tp: prog4'
    create_atffile dir3/Atffile 'prop: test-suite = "foo"' 'tp: prog1'
    cd -

    atf_check -s exit:0 -e ignore atf2kyua "${@}"

    cd "${target_root}"
    cat >expected <<EOF
syntax('kyuafile', 1)

test_suite('foo')

include('dir1/Kyuafile')

atf_test_program{name='prog1'}
EOF
    atf_check -s exit:0 -o file:expected cat Kyuafile

    cat >expected <<EOF
syntax('kyuafile', 1)

test_suite('bar')

include('dir2/Kyuafile')

atf_test_program{name='prog2'}
EOF
    atf_check -s exit:0 -o file:expected cat dir1/Kyuafile

    cat >expected <<EOF
syntax('kyuafile', 1)

test_suite('foo')

atf_test_program{name='prog3'}
atf_test_program{name='prog4'}
EOF
    atf_check -s exit:0 -o file:expected cat dir1/dir2/Kyuafile

    cat >expected <<EOF
syntax('kyuafile', 1)

test_suite('foo')

atf_test_program{name='prog1'}
EOF
    atf_check -s exit:0 -o file:expected cat dir3/Kyuafile
    cd -
}


atf_test_case default__one_level
default__one_level_body() {
    one_level_test . .
}


atf_test_case default__multiple_levels
default__multiple_levels_body() {
    multiple_levels_test . .
}


atf_test_case sflag__one_level
sflag__one_level_body() {
    mkdir root
    one_level_test root root -s root
}


atf_test_case sflag__multiple_levels
sflag__multiple_levels_body() {
    mkdir root
    multiple_levels_test root root -s root
}


atf_test_case tflag__one_level
tflag__one_level_body() {
    one_level_test . target -t target
}


atf_test_case tflag__multiple_levels
tflag__multiple_levels_body() {
    multiple_levels_test . target -t target
}


atf_test_case sflag_tflag__one_level
sflag_tflag__one_level_body() {
    mkdir source
    one_level_test source target -s source -t target
}


atf_test_case sflag_tflag__multiple_levels
sflag_tflag__multiple_levels_body() {
    mkdir source
    multiple_levels_test source target -s source -t target
}


atf_test_case prune_stale_kyuafiles
prune_stale_kyuafiles_body() {
    create_test_program p1 fail
    create_test_program p2 skip
    chmod +x p1 p2
    create_atffile Atffile 'prop: test-suite = "foo"' 'tp: p1' 'tp: p2'

    mkdir subdir
    touch subdir/Kyuafile

    atf_check -s exit:0 -e ignore atf2kyua

    test -f Kyuafile || atf_fail "Kyuafiles not created as expected"
    test \! -f subdir/Kyuafile || atf_fail "Stale kyuafiles not removed"
}


atf_test_case unknown_option
unknown_option_body() {
    atf_check -s exit:1 -o empty -e match:'E: Unknown option -Z' \
        atf2kyua -Z -A
}


atf_test_case too_many_arguments
too_many_arguments_body() {
    atf_check -s exit:1 -o empty -e match:'E: No arguments allowed' \
        atf2kyua first
}


atf_init_test_cases() {
    atf_add_test_case default__one_level
    atf_add_test_case default__multiple_levels

    atf_add_test_case sflag__one_level
    atf_add_test_case sflag__multiple_levels

    atf_add_test_case tflag__one_level
    atf_add_test_case tflag__multiple_levels

    atf_add_test_case sflag_tflag__one_level
    atf_add_test_case sflag_tflag__multiple_levels

    atf_add_test_case prune_stale_kyuafiles

    atf_add_test_case unknown_option
    atf_add_test_case too_many_arguments
}
