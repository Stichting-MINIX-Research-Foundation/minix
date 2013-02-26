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

#include "result.h"

#include <sys/stat.h>

#include <atf-c.h>

#include "error.h"


ATF_TC_WITHOUT_HEAD(write__passed);
ATF_TC_BODY(write__passed, tc)
{
    const kyua_error_t error = kyua_result_write(
        "test.txt", KYUA_RESULT_PASSED, NULL);
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE(atf_utils_compare_file("test.txt", "passed\n"));
}


ATF_TC_WITHOUT_HEAD(write__failed);
ATF_TC_BODY(write__failed, tc)
{
    const kyua_error_t error = kyua_result_write(
        "test.txt", KYUA_RESULT_FAILED, "Some message");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE(atf_utils_compare_file("test.txt", "failed: Some message\n"));
}


ATF_TC_WITHOUT_HEAD(write__broken);
ATF_TC_BODY(write__broken, tc)
{
    const kyua_error_t error = kyua_result_write(
        "test.txt", KYUA_RESULT_BROKEN, "Some message");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE(atf_utils_compare_file("test.txt", "broken: Some message\n"));
}


ATF_TC_WITHOUT_HEAD(write__skipped);
ATF_TC_BODY(write__skipped, tc)
{
    const kyua_error_t error = kyua_result_write(
        "test.txt", KYUA_RESULT_SKIPPED, "Some message");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE(atf_utils_compare_file("test.txt", "skipped: Some message\n"));
}


ATF_TC_WITHOUT_HEAD(write__expected_failure);
ATF_TC_BODY(write__expected_failure, tc)
{
    const kyua_error_t error = kyua_result_write(
        "test.txt", KYUA_RESULT_EXPECTED_FAILURE, "Some message");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE(atf_utils_compare_file("test.txt", "expected_failure: "
                                       "Some message\n"));
}


ATF_TC(write__open_error);
ATF_TC_HEAD(write__open_error, tc)
{
    atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(write__open_error, tc)
{
    ATF_REQUIRE(mkdir("readonly", 0555) != -1);
    const kyua_error_t error = kyua_result_write(
        "readonly/test.txt", KYUA_RESULT_FAILED, "Some message");
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    char buffer[512];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE(atf_utils_grep_string(
        "Cannot create result file 'readonly/test.txt'", buffer));
    kyua_error_free(error);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, write__passed);
    ATF_TP_ADD_TC(tp, write__failed);
    ATF_TP_ADD_TC(tp, write__skipped);
    ATF_TP_ADD_TC(tp, write__broken);
    ATF_TP_ADD_TC(tp, write__expected_failure);
    ATF_TP_ADD_TC(tp, write__open_error);

    return atf_no_error();
}
