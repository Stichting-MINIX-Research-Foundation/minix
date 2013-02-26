// Copyright 2012 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "atf_list.h"

#include <fcntl.h>

#include <atf-c.h>

#include "error.h"


/// Opens a file for reading and fails the test case if the open fails.
///
/// \param name The file to open.
///
/// \return A valid file descriptor.
static int
safe_open(const char* name)
{
    const int fd = open(name, O_RDONLY);
    ATF_REQUIRE(fd != -1);
    return fd;
}


/// Test atf_list_parse for a valid case.
///
/// \param input_name Result file to parse.
/// \param exp_output Expected output of the parsing.
static void
do_ok_test(const char* input_name, const char* exp_output)
{
    FILE* output = fopen("output.txt", "w");
    kyua_error_t error = atf_list_parse(safe_open(input_name), output);
    fclose(output);

    if (!atf_utils_compare_file("output.txt", exp_output)) {
        atf_utils_create_file("expout.txt", "%s", exp_output);
        atf_utils_cat_file("expout.txt", "EXPECTED: ");
        atf_utils_cat_file("output.txt", "ACTUAL:   ");
        atf_tc_fail_nonfatal("Output of atf_list_parse does not match expected "
                             "results");
    }

    if (kyua_error_is_set(error)) {
        char message[1024];
        kyua_error_format(error, message, sizeof(message));
        kyua_error_free(error);
        atf_tc_fail("%s", message);
    }
}


/// Test atf_list_parse for an error case.
///
/// \param input_name Invalid result file to parse.
/// \param exp_type Expected error type.
/// \param exp_message Expected error message.
static void
do_fail_test(const char* input_name, const char* exp_type,
             const char* exp_message)
{
    FILE* output = fopen("output.txt", "w");
    kyua_error_t error = atf_list_parse(safe_open(input_name), output);
    fclose(output);

    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, exp_type));

    char message[1024];
    kyua_error_format(error, message, sizeof(message));
    kyua_error_free(error);
    ATF_REQUIRE_MATCH(exp_message, message);
}


ATF_TC_WITHOUT_HEAD(parse__ok__one);
ATF_TC_BODY(parse__ok__one, tc)
{
    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n");
    do_ok_test(
        "input.txt",
        "test_case{name='first'}\n");
}


ATF_TC_WITHOUT_HEAD(parse__ok__several);
ATF_TC_BODY(parse__ok__several, tc)
{
    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "require.user: root\n"
        "\n"
        "ident: second\n"
        "\n"
        "ident: third\n"
        "descr: A string with an embedded ' and \\' in it\n"
        "has.cleanup: true\n"
        "X-custom: foo\n"
        "X-a'b: bar\n");
    do_ok_test(
        "input.txt",
        "test_case{name='first', required_user='root'}\n"
        "test_case{name='second'}\n"
        "test_case{name='third', description='A string with an embedded \\' "
        "and \\\\\\' in it', has_cleanup='true', ['custom.X-custom']='foo', "
        "['custom.X-a\\'b']='bar'}\n");
}


ATF_TC_WITHOUT_HEAD(parse__error__bad_fd);
ATF_TC_BODY(parse__error__bad_fd, tc)
{
    (void)close(10);
    kyua_error_t error = atf_list_parse(10, stdout);

    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));

    char message[1024];
    kyua_error_format(error, message, sizeof(message));
    kyua_error_free(error);
    ATF_REQUIRE_MATCH("fdopen\\(10\\) failed", message);
}


ATF_TC_WITHOUT_HEAD(parse__error__bad_header);
ATF_TC_BODY(parse__error__bad_header, tc)
{
    atf_utils_create_file("input.txt", "%s", "");
    do_fail_test("input.txt", "generic",
                 "fgets failed to read test cases list header");

    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1234\"");
    do_fail_test("input.txt", "generic",
                 "Invalid test cases list header '.*X-atf-tp.*1234.*'");

    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n");
    do_fail_test("input.txt", "generic",
                 "fgets failed to read test cases list header");

    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "some garbage\n"
        "\n");
    do_fail_test("input.txt", "generic",
                 "Incomplete test cases list header");
}


ATF_TC_WITHOUT_HEAD(parse__error__empty);
ATF_TC_BODY(parse__error__empty, tc)
{
    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n");
    do_fail_test("input.txt", "generic",
                 "Empty test cases list: unexpected EOF");
}


ATF_TC_WITHOUT_HEAD(parse__error__bad_property);
ATF_TC_BODY(parse__error__bad_property, tc)
{
    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "oh noes; 2\n");
    do_fail_test("input.txt", "generic", "Invalid property 'oh noes; 2'");
}


ATF_TC_WITHOUT_HEAD(parse__error__bad_order);
ATF_TC_BODY(parse__error__bad_order, tc)
{
    atf_utils_create_file(
        "input.txt",
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "descr: Some text\n"
        "ident: first\n");
    do_fail_test("input.txt", "generic", "Expected ident property, got descr");
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, parse__ok__one);
    ATF_TP_ADD_TC(tp, parse__ok__several);
    ATF_TP_ADD_TC(tp, parse__error__bad_fd);
    ATF_TP_ADD_TC(tp, parse__error__bad_header);
    ATF_TP_ADD_TC(tp, parse__error__empty);
    ATF_TP_ADD_TC(tp, parse__error__bad_property);
    ATF_TP_ADD_TC(tp, parse__error__bad_order);

    return atf_no_error();
}
