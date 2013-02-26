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


utils_test_case defaults
defaults_body() {
    atf_check -s exit:0 \
        -o match:'^architecture = ' \
        -o match:'^platform = ' \
        kyua config
}


utils_test_case all
all_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
architecture = "my-architecture"
platform = "my-platform"
unprivileged_user = "$(id -u -n)"
test_suites.suite1["X-the-variable"] = "value1"
test_suites.suite2["X-the-variable"] = "value2"
EOF

    cat >expout <<EOF
architecture = my-architecture
platform = my-platform
test_suites.suite1.X-the-variable = value1
test_suites.suite2.X-the-variable = value2
unprivileged_user = $(id -u -n)
EOF

    atf_check -s exit:0 -o file:expout -e empty kyua config
}


utils_test_case one__ok
one__ok_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
test_suites.first["X-one"] = 1
test_suites.first["X-two"] = 2
EOF

    cat >expout <<EOF
test_suites.first.X-two = 2
EOF

    atf_check -s exit:0 -o file:expout -e empty kyua config \
        test_suites.first.X-two
}


utils_test_case one__fail
one__fail_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
test_suites.first["X-one"] = 1
test_suites.first["X-three"] = 3
EOF

    cat >experr <<EOF
kyua: W: 'test_suites.first.X-two' is not defined.
EOF

    atf_check -s exit:1 -o empty -e file:experr kyua config \
        test_suites.first.X-two
}


utils_test_case many__ok
many__ok_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
test_suites.first["X-one"] = 1
test_suites.first["X-two"] = 2
EOF

    cat >expout <<EOF
test_suites.first.X-two = 2
test_suites.first.X-one = 1
EOF

    atf_check -s exit:0 -o file:expout -e empty kyua config \
        test_suites.first.X-two \
        test_suites.first.X-one  # Inverse order on purpose.
}


utils_test_case many__fail
many__fail_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
test_suites.first["X-one"] = 1
test_suites.first["X-three"] = 3
EOF

    cat >expout <<EOF
test_suites.first.X-one = 1
test_suites.first.X-three = 3
EOF

    cat >experr <<EOF
kyua: W: 'test_suites.first.X-two' is not defined.
kyua: W: 'test_suites.first.X-fourth' is not defined.
EOF

    atf_check -s exit:1 -o file:expout -e file:experr kyua config \
        test_suites.first.X-one test_suites.first.X-two \
        test_suites.first.X-three test_suites.first.X-fourth
}


utils_test_case config_flag__default_system
config_flag__default_system_body() {
    cat >kyua.conf <<EOF
syntax(2)
test_suites.foo["X-var"] = "baz"
EOF

    atf_check -s exit:1 -o empty \
        -e match:"kyua: W: 'test_suites.foo.X-var'.*not defined" \
        kyua config test_suites.foo.X-var
    export KYUA_CONFDIR="$(pwd)"
    atf_check -s exit:0 -o match:"foo.X-var = baz" -e empty \
        kyua config test_suites.foo.X-var
}


utils_test_case config_flag__default_home
config_flag__default_home_body() {
    cat >kyua.conf <<EOF
syntax(2)
test_suites.foo["X-var"] = "bar"
EOF
    export KYUA_CONFDIR="$(pwd)"
    atf_check -s exit:0 -o match:"test_suites.foo.X-var = bar" -e empty \
        kyua config test_suites.foo.X-var

    # The previously-created "system-wide" file has to be ignored.
    mkdir .kyua
    cat >.kyua/kyua.conf <<EOF
syntax(2)
test_suites.foo["X-var"] = "baz"
EOF
    atf_check -s exit:0 -o match:"test_suites.foo.X-var = baz" -e empty \
        kyua config test_suites.foo.X-var
}


utils_test_case config_flag__explicit__ok
config_flag__explicit__ok_body() {
    cat >kyua.conf <<EOF
syntax(2)
test_suites.foo["X-var"] = "baz"
EOF

    atf_check -s exit:1 -o empty \
        -e match:"kyua: W: 'test_suites.foo.X-var'.*not defined" \
        kyua config test_suites.foo.X-var
    atf_check -s exit:0 -o match:"test_suites.foo.X-var = baz" -e empty \
        kyua -c kyua.conf config test_suites.foo.X-var
    atf_check -s exit:0 -o match:"test_suites.foo.X-var = baz" -e empty \
        kyua --config=kyua.conf config test_suites.foo.X-var
}


utils_test_case config_flag__explicit__disable
config_flag__explicit__disable_body() {
    cat >kyua.conf <<EOF
syntax(2)
test_suites.foo["X-var"] = "baz"
EOF
    mkdir .kyua
    cp kyua.conf .kyua/kyua.conf
    export KYUA_CONFDIR="$(pwd)"

    atf_check -s exit:0 -o match:"test_suites.foo.X-var = baz" -e empty \
        kyua config test_suites.foo.X-var
    atf_check -s exit:1 -o empty \
        -e match:"kyua: W: 'test_suites.foo.X-var'.*not defined" \
        kyua --config=none config test_suites.foo.X-var
}


utils_test_case config_flag__explicit__missing_file
config_flag__explicit__missing_file_body() {
    cat >experr <<EOF
kyua: E: Load of 'foo' failed: File 'foo' not found.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua --config=foo config
}


utils_test_case config_flag__explicit__bad_file
config_flag__explicit__bad_file_body() {
    touch custom
    atf_check -s exit:2 -o empty -e match:"No syntax defined" \
        kyua --config=custom config
}


utils_test_case variable_flag__no_config
variable_flag__no_config_body() {
    atf_check -s exit:0 \
        -o match:'test_suites.suite1.X-the-variable = value1' \
        -o match:'test_suites.suite2.X-the-variable = value2' \
        -e empty \
        kyua \
        -v "test_suites.suite1.X-the-variable=value1" \
        -v "test_suites.suite2.X-the-variable=value2" \
        config

    atf_check -s exit:0 \
        -o match:'test_suites.suite1.X-the-variable = value1' \
        -o match:'test_suites.suite2.X-the-variable = value2' \
        -e empty \
        kyua \
        --variable="test_suites.suite1.X-the-variable=value1" \
        --variable="test_suites.suite2.X-the-variable=value2" \
        config
}


utils_test_case variable_flag__override_default_config
variable_flag__override_default_config_body() {
    mkdir "${HOME}/.kyua"
    cat >"${HOME}/.kyua/kyua.conf" <<EOF
syntax(2)
test_suites.suite1["X-the-variable"] = "value1"
test_suites.suite2["X-the-variable"] = "should not be used"
EOF

    atf_check -s exit:0 \
        -o match:'test_suites.suite1.X-the-variable = value1' \
        -o match:'test_suites.suite2.X-the-variable = overriden' \
        -o match:'test_suites.suite3.X-the-variable = new' \
        -e empty kyua \
        -v "test_suites.suite2.X-the-variable=overriden" \
        -v "test_suites.suite3.X-the-variable=new" \
        config

    atf_check -s exit:0 \
        -o match:'test_suites.suite1.X-the-variable = value1' \
        -o match:'test_suites.suite2.X-the-variable = overriden' \
        -o match:'test_suites.suite3.X-the-variable = new' \
        -e empty kyua \
        --variable="test_suites.suite2.X-the-variable=overriden" \
        --variable="test_suites.suite3.X-the-variable=new" \
        config
}


utils_test_case variable_flag__override_custom_config
variable_flag__override_custom_config_body() {
    cat >config <<EOF
syntax(2)
test_suites.suite1["X-the-variable"] = "value1"
test_suites.suite2["X-the-variable"] = "should not be used"
EOF

    atf_check -s exit:0 \
        -o match:'test_suites.suite2.X-the-variable = overriden' \
        -e empty kyua -c config \
        -v "test_suites.suite2.X-the-variable=overriden" config

    atf_check -s exit:0 \
        -o match:'test_suites.suite2.X-the-variable = overriden' \
        -e empty kyua -c config \
        --variable="test_suites.suite2.X-the-variable=overriden" config
}


utils_test_case variable_flag__invalid
variable_flag__invalid_body() {
    cat >experr <<EOF
Usage error: Invalid argument '' for option --variable: Argument does not have the form 'K=V'.
Type 'kyua help' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:experr kyua \
        -v "test_suites.a.b=c" -v "" config

    cat >experr <<EOF
kyua: E: Unknown configuration property 'foo'.
EOF
    atf_check -s exit:2 -o empty -e file:experr kyua \
        -v "test_suites.a.b=c" -v "foo=bar" config
}


atf_init_test_cases() {
    atf_add_test_case defaults
    atf_add_test_case all
    atf_add_test_case one__ok
    atf_add_test_case one__fail
    atf_add_test_case many__ok
    atf_add_test_case many__fail

    atf_add_test_case config_flag__default_system
    atf_add_test_case config_flag__default_home
    atf_add_test_case config_flag__explicit__ok
    atf_add_test_case config_flag__explicit__disable
    atf_add_test_case config_flag__explicit__missing_file
    atf_add_test_case config_flag__explicit__bad_file

    atf_add_test_case variable_flag__no_config
    atf_add_test_case variable_flag__override_default_config
    atf_add_test_case variable_flag__override_custom_config
    atf_add_test_case variable_flag__invalid
}
