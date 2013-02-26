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

#include "error.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>


ATF_TC_WITHOUT_HEAD(error_new__oom);
ATF_TC_BODY(error_new__oom, tc)
{
    void* invalid = (void*)1;
    kyua_error_t error = kyua_error_new("test_error", invalid, SIZE_MAX, NULL);
    ATF_REQUIRE(kyua_error_is_type(error, kyua_oom_error_type));
    ATF_REQUIRE(kyua_error_data(error) == NULL);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_subsume__none);
ATF_TC_BODY(error_subsume__none, tc)
{
    kyua_error_t primary = kyua_error_ok();
    kyua_error_t secondary = kyua_error_ok();
    ATF_REQUIRE(!kyua_error_is_set(kyua_error_subsume(primary, secondary)));
}


ATF_TC_WITHOUT_HEAD(error_subsume__primary);
ATF_TC_BODY(error_subsume__primary, tc)
{
    kyua_error_t primary = kyua_error_new("primary_error", NULL, 0, NULL);
    kyua_error_t secondary = kyua_error_new("secondary_error", NULL, 0, NULL);
    kyua_error_t error = kyua_error_subsume(primary, secondary);
    ATF_REQUIRE(kyua_error_is_type(error, "primary_error"));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_subsume__secondary);
ATF_TC_BODY(error_subsume__secondary, tc)
{
    kyua_error_t primary = kyua_error_ok();
    kyua_error_t secondary = kyua_error_new("secondary_error", NULL, 0, NULL);
    kyua_error_t error = kyua_error_subsume(primary, secondary);
    ATF_REQUIRE(kyua_error_is_type(error, "secondary_error"));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_is_type__match);
ATF_TC_BODY(error_is_type__match, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, NULL);
    ATF_REQUIRE(kyua_error_is_type(error, "test_error"));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_is_type__not_match);
ATF_TC_BODY(error_is_type__not_match, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, NULL);
    ATF_REQUIRE(!kyua_error_is_type(error, "test_erro"));
    ATF_REQUIRE(!kyua_error_is_type(error, "test_error2"));
    ATF_REQUIRE(!kyua_error_is_type(error, "foo"));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_data__none);
ATF_TC_BODY(error_data__none, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, NULL);
    ATF_REQUIRE(kyua_error_data(error) == NULL);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_data__some);
ATF_TC_BODY(error_data__some, tc)
{
    int data = 5;
    kyua_error_t error = kyua_error_new("test_data_error", &data, sizeof(data),
                                        NULL);
    ATF_REQUIRE(kyua_error_data(error) != NULL);
    ATF_REQUIRE_EQ(*((const int*)kyua_error_data(error)), 5);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_is_set__no);
ATF_TC_BODY(error_is_set__no, tc)
{
    kyua_error_t error = kyua_error_ok();
    ATF_REQUIRE(!kyua_error_is_set(error));
}


ATF_TC_WITHOUT_HEAD(error_is_set__yes);
ATF_TC_BODY(error_is_set__yes, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, NULL);
    ATF_REQUIRE(kyua_error_is_set(error));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_format__default);
ATF_TC_BODY(error_format__default, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, NULL);
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("Error 'test_error'", buffer);
    kyua_error_free(error);
}


/// Error-specific formatting function for testing purposes.
static int
test_format(const kyua_error_t error, char* const output_buffer,
            const size_t output_size)
{
    ATF_REQUIRE(kyua_error_is_type(error, "test_error"));
    return snprintf(output_buffer, output_size, "Test formatting function");
}


ATF_TC_WITHOUT_HEAD(error_format__custom__ok);
ATF_TC_BODY(error_format__custom__ok, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, test_format);
    const char* exp_message = "Test formatting function";
    char buffer[1024];
    ATF_REQUIRE_EQ((int)strlen(exp_message),
                   kyua_error_format(error, buffer, sizeof(buffer)));
    ATF_REQUIRE_STREQ(exp_message, buffer);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(error_format__custom__error);
ATF_TC_BODY(error_format__custom__error, tc)
{
    kyua_error_t error = kyua_error_new("test_error", NULL, 0, test_format);
    char buffer[5];
    ATF_REQUIRE(kyua_error_format(error, buffer, sizeof(buffer))
                >= (int)sizeof(buffer));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(err);
ATF_TC_BODY(err, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        kyua_error_t error = kyua_usage_error_new("A usage error");
        kyua_error_err(15, error, "The %s message", "1st");
    }
    atf_utils_wait(pid, 15, "", "error_test: The 1st message: A usage error\n");
}


ATF_TC_WITHOUT_HEAD(fprintf);
ATF_TC_BODY(fprintf, tc)
{
    FILE* output = fopen("output", "w");
    const kyua_error_t error = kyua_usage_error_new("A usage error");
    kyua_error_fprintf(output, error, "The %s message", "1st");
    kyua_error_free(error);
    fclose(output);

    ATF_REQUIRE(atf_utils_grep_file("The 1st message: A usage error",
                                    "output"));
}


ATF_TC_WITHOUT_HEAD(warn);
ATF_TC_BODY(warn, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        kyua_error_t error = kyua_usage_error_new("A usage error");
        kyua_error_warn(error, "The %s message", "1st");
        kyua_error_free(error);
        exit(51);
    }
    atf_utils_wait(pid, 51, "", "error_test: The 1st message: A usage error\n");
}


ATF_TC_WITHOUT_HEAD(generic_error_type);
ATF_TC_BODY(generic_error_type, tc)
{
    kyua_error_t error = kyua_generic_error_new("Nothing");
    ATF_REQUIRE(kyua_error_is_type(error, kyua_generic_error_type));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(generic_error_format__plain);
ATF_TC_BODY(generic_error_format__plain, tc)
{
    kyua_error_t error = kyua_generic_error_new("Test message");
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("Test message", buffer);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(generic_error_format__args);
ATF_TC_BODY(generic_error_format__args, tc)
{
    kyua_error_t error = kyua_generic_error_new("%s message %d", "A", 123);
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("A message 123", buffer);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(libc_error_type);
ATF_TC_BODY(libc_error_type, tc)
{
    kyua_error_t error = kyua_libc_error_new(ENOMEM, "Nothing");
    ATF_REQUIRE(kyua_error_is_type(error, kyua_libc_error_type));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(libc_error_errno);
ATF_TC_BODY(libc_error_errno, tc)
{
    kyua_error_t error = kyua_libc_error_new(EPERM, "Doesn't matter");
    ATF_REQUIRE_EQ(EPERM, kyua_libc_error_errno(error));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(libc_error_format__plain);
ATF_TC_BODY(libc_error_format__plain, tc)
{
    kyua_error_t error = kyua_libc_error_new(ENOMEM, "Test message");
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE(strstr(buffer, strerror(ENOMEM)) != NULL);
    ATF_REQUIRE(strstr(buffer, "Test message") != NULL);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(libc_error_format__args);
ATF_TC_BODY(libc_error_format__args, tc)
{
    kyua_error_t error = kyua_libc_error_new(EPERM, "%s message %d", "A", 123);
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE(strstr(buffer, strerror(EPERM)) != NULL);
    ATF_REQUIRE(strstr(buffer, "A message 123") != NULL);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(oom_error_type);
ATF_TC_BODY(oom_error_type, tc)
{
    kyua_error_t error = kyua_oom_error_new();
    ATF_REQUIRE(kyua_error_is_type(error, kyua_oom_error_type));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(oom_error_data);
ATF_TC_BODY(oom_error_data, tc)
{
    kyua_error_t error = kyua_oom_error_new();
    ATF_REQUIRE(kyua_error_data(error) == NULL);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(oom_error_format);
ATF_TC_BODY(oom_error_format, tc)
{
    kyua_error_t error = kyua_oom_error_new();
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("Not enough memory", buffer);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(oom_error_reuse);
ATF_TC_BODY(oom_error_reuse, tc)
{
    {
        kyua_error_t error = kyua_oom_error_new();
        ATF_REQUIRE(kyua_error_is_type(error, kyua_oom_error_type));
        ATF_REQUIRE(kyua_error_data(error) == NULL);
        kyua_error_free(error);
    }

    {
        kyua_error_t error = kyua_oom_error_new();
        ATF_REQUIRE(kyua_error_is_type(error, kyua_oom_error_type));
        ATF_REQUIRE(kyua_error_data(error) == NULL);
        kyua_error_free(error);
    }
}


ATF_TC_WITHOUT_HEAD(usage_error_type);
ATF_TC_BODY(usage_error_type, tc)
{
    kyua_error_t error = kyua_usage_error_new("Nothing");
    ATF_REQUIRE(kyua_error_is_type(error, kyua_usage_error_type));
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(usage_error_format__plain);
ATF_TC_BODY(usage_error_format__plain, tc)
{
    kyua_error_t error = kyua_usage_error_new("Test message");
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("Test message", buffer);
    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(usage_error_format__args);
ATF_TC_BODY(usage_error_format__args, tc)
{
    kyua_error_t error = kyua_usage_error_new("%s message %d", "A", 123);
    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("A message 123", buffer);
    kyua_error_free(error);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, error_new__oom);
    ATF_TP_ADD_TC(tp, error_subsume__none);
    ATF_TP_ADD_TC(tp, error_subsume__primary);
    ATF_TP_ADD_TC(tp, error_subsume__secondary);
    ATF_TP_ADD_TC(tp, error_is_type__match);
    ATF_TP_ADD_TC(tp, error_is_type__not_match);
    ATF_TP_ADD_TC(tp, error_data__none);
    ATF_TP_ADD_TC(tp, error_data__some);
    ATF_TP_ADD_TC(tp, error_is_set__no);
    ATF_TP_ADD_TC(tp, error_is_set__yes);
    ATF_TP_ADD_TC(tp, error_format__default);
    ATF_TP_ADD_TC(tp, error_format__custom__ok);
    ATF_TP_ADD_TC(tp, error_format__custom__error);

    ATF_TP_ADD_TC(tp, err);
    ATF_TP_ADD_TC(tp, fprintf);
    ATF_TP_ADD_TC(tp, warn);

    ATF_TP_ADD_TC(tp, generic_error_type);
    ATF_TP_ADD_TC(tp, generic_error_format__plain);
    ATF_TP_ADD_TC(tp, generic_error_format__args);

    ATF_TP_ADD_TC(tp, libc_error_type);
    ATF_TP_ADD_TC(tp, libc_error_errno);
    ATF_TP_ADD_TC(tp, libc_error_format__plain);
    ATF_TP_ADD_TC(tp, libc_error_format__args);

    ATF_TP_ADD_TC(tp, oom_error_type);
    ATF_TP_ADD_TC(tp, oom_error_data);
    ATF_TP_ADD_TC(tp, oom_error_format);
    ATF_TP_ADD_TC(tp, oom_error_reuse);

    ATF_TP_ADD_TC(tp, usage_error_type);
    ATF_TP_ADD_TC(tp, usage_error_format__plain);
    ATF_TP_ADD_TC(tp, usage_error_format__args);

    return atf_no_error();
}
