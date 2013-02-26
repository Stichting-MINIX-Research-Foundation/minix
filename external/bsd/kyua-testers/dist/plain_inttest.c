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

#include <stdlib.h>

#include <atf-c.h>

#define INTERFACE "plain"
#include "common_inttest.h"


/// Sets up a particular helper.
///
/// \param tc The test case calling this function.
/// \param helper_name Name of the desired helper.
///
/// \return The return value of helpers_path().
static char*
select_helper(const atf_tc_t* tc, const char* helper_name)
{
    ATF_REQUIRE(setenv("HELPER", helper_name, 1) != -1);
    return helpers_path(tc);
}


ATF_TC(list__ok);
ATF_TC_HEAD(list__ok, tc) { setup(tc, false); }
ATF_TC_BODY(list__ok, tc)
{
    check(EXIT_SUCCESS, "test_case{name='main'}\n", "",
          "list", "irrelevant-program", NULL);
}


ATF_TC(test__pass);
ATF_TC_HEAD(test__pass, tc) { setup(tc, true); }
ATF_TC_BODY(test__pass, tc)
{
    char* helpers = select_helper(tc, "pass");
    check(EXIT_SUCCESS,
          "First line to stdout\nSecond line to stdout\n",
          "First line to stderr\nSecond line to stderr\n",
          "test", helpers, "main", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__fail);
ATF_TC_HEAD(test__fail, tc) { setup(tc, true); }
ATF_TC_BODY(test__fail, tc)
{
    char* helpers = select_helper(tc, "fail");
    check(EXIT_FAILURE, "First line to stdout\n", "First line to stderr\n",
          "test", helpers, "main", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "failed: Returned non-success exit status 78\n"));
}


ATF_TC(test__crash);
ATF_TC_HEAD(test__crash, tc) { setup(tc, true); }
ATF_TC_BODY(test__crash, tc)
{
    char* helpers = select_helper(tc, "signal");
    check(EXIT_FAILURE, "", "save:crash.err",
          "test", helpers, "main", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "broken: Received signal 6\n"));

    ATF_REQUIRE(atf_utils_grep_file("About to die due to SIGABRT!",
                                    "crash.err"));
    ATF_REQUIRE(atf_utils_grep_file("attempting to gather stack trace",
                                    "crash.err"));
}


ATF_TC(test__timeout);
ATF_TC_HEAD(test__timeout, tc) { setup(tc, true); }
ATF_TC_BODY(test__timeout, tc)
{
    char* helpers = select_helper(tc, "sleep");
    check(EXIT_FAILURE, "", "Subprocess timed out; sending KILL signal...\n",
          "-t1", "test", helpers, "main", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "broken: Test case "
                                       "timed out\n"));
}


ATF_TC(test__config_ignored);
ATF_TC_HEAD(test__config_ignored, tc) { setup(tc, true); }
ATF_TC_BODY(test__config_ignored, tc)
{
    char* helpers = select_helper(tc, "pass");
    check(EXIT_SUCCESS,
          "First line to stdout\nSecond line to stdout\n",
          "save:stderr.txt",
          "test", "-va=b", "-vfoo=a b c", helpers, "main", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_grep_file("ignoring 'a=b'", "stderr.txt"));
    ATF_REQUIRE(atf_utils_grep_file("ignoring 'foo=a b c'", "stderr.txt"));
    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__missing_test_program);
ATF_TC_HEAD(test__missing_test_program, tc) { setup(tc, false); }
ATF_TC_BODY(test__missing_test_program, tc)
{
    check(EXIT_INTERNAL_ERROR, "",
          "kyua-plain-tester: execvp failed: No such file or directory\n",
          "test", "./non-existent", "main", "test-result", NULL);

    ATF_REQUIRE(!atf_utils_file_exists("test-result"));
}


ATF_TC(test__invalid_test_case_name);
ATF_TC_HEAD(test__invalid_test_case_name, tc) { setup(tc, false); }
ATF_TC_BODY(test__invalid_test_case_name, tc)
{
    check(EXIT_INTERNAL_ERROR, "",
          "kyua-plain-tester: Unknown test case 'foo'\n",
          "test", "./non-existent", "foo", "test-result", NULL);

    ATF_REQUIRE(!atf_utils_file_exists("test-result"));
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, top__missing_command);
    ATF_TP_ADD_TC(tp, top__unknown_command);

    ATF_TP_ADD_TC(tp, list__ok);

    ATF_TP_ADD_TC(tp, test__pass);
    ATF_TP_ADD_TC(tp, test__fail);
    ATF_TP_ADD_TC(tp, test__crash);
    ATF_TP_ADD_TC(tp, test__timeout);
    ATF_TP_ADD_TC(tp, test__config_ignored);
    ATF_TP_ADD_TC(tp, test__missing_test_program);
    ATF_TP_ADD_TC(tp, test__invalid_test_case_name);

    return atf_no_error();
}
