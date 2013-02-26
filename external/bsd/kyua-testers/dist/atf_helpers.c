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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>


ATF_TC_WITHOUT_HEAD(fail);
ATF_TC_BODY(fail, tc)
{
    fprintf(stdout, "First line to stdout\n");
    fprintf(stderr, "First line to stderr\n");
    atf_tc_fail("This is the failure message");
}


ATF_TC_WITHOUT_HEAD(pass);
ATF_TC_BODY(pass, tc)
{
    fprintf(stdout, "First line to stdout\n");
    fprintf(stdout, "Second line to stdout\n");
    fprintf(stderr, "First line to stderr\n");
    fprintf(stderr, "Second line to stderr\n");
}


ATF_TC_WITHOUT_HEAD(signal);
ATF_TC_BODY(signal, tc)
{
    fprintf(stderr, "About to die due to SIGABRT!\n");
    abort();
}


ATF_TC_WITHOUT_HEAD(sleep);
ATF_TC_BODY(sleep, tc)
{
    sleep(300);
}


ATF_TC_WITH_CLEANUP(cleanup_check_work_directory);
ATF_TC_HEAD(cleanup_check_work_directory, tc) { }
ATF_TC_BODY(cleanup_check_work_directory, tc)
{
    fprintf(stdout, "Body stdout\n");
    fprintf(stderr, "Body stderr\n");
    atf_utils_create_file("cookie-in-work-directory", "the value\n");
}
ATF_TC_CLEANUP(cleanup_check_work_directory, tc)
{
    fprintf(stdout, "Cleanup stdout\n");
    fprintf(stderr, "Cleanup stderr\n");
    if (atf_utils_compare_file("cookie-in-work-directory", "the value\n")) {
        printf("Cleanup properly ran in the same directory as the body\n");
    } else {
        printf("Cleanup did not run in the same directory as the body\n");
    }
}


ATF_TC_WITH_CLEANUP(cleanup_fail);
ATF_TC_HEAD(cleanup_fail, tc) { }
ATF_TC_BODY(cleanup_fail, tc)
{
}
ATF_TC_CLEANUP(cleanup_fail, tc)
{
    exit(EXIT_FAILURE);
}


ATF_TC_WITH_CLEANUP(cleanup_signal);
ATF_TC_HEAD(cleanup_signal, tc) { }
ATF_TC_BODY(cleanup_signal, tc) { }
ATF_TC_CLEANUP(cleanup_signal, tc)
{
    fprintf(stderr, "About to die due to SIGABRT!\n");
    abort();
}


ATF_TC_WITH_CLEANUP(cleanup_sleep);
ATF_TC_HEAD(cleanup_sleep, tc) { }
ATF_TC_BODY(cleanup_sleep, tc) { }
ATF_TC_CLEANUP(cleanup_sleep, tc)
{
    sleep(300);
}


ATF_TC_WITH_CLEANUP(body_and_cleanup_fail);
ATF_TC_HEAD(body_and_cleanup_fail, tc) { }
ATF_TC_BODY(body_and_cleanup_fail, tc)
{
    atf_tc_fail("Body fails");
}
ATF_TC_CLEANUP(body_and_cleanup_fail, tc)
{
    printf("Killing cleanup\n");
    fflush(stdout);
    kill(getpid(), SIGKILL);
}


/// Prints a configuration variable if it exists.
///
/// \param tc Caller test case.
/// \param part Identifier for the part of the test case.
/// \param name Name of the configuration variable.
static void
print_config_var(const atf_tc_t* tc, const char* part, const char* name)
{
    if (atf_tc_has_config_var(tc, name))
        printf("%s %s %s\n", part, name, atf_tc_get_config_var(tc, name));
}


/// Prints the configuration variables of the test case.
///
/// Ideally we'd print all variables but the ATF interface does not have an API
/// to iterate over them all.  Instead, this just prints the variables we are
/// interested in for testing purposes.
///
/// \param tc Caller test case.
/// \param part Identifier for the part of the test case.
static void
print_config_vars(const atf_tc_t* tc, const char* part)
{
    print_config_var(tc, part, "my-var1");
    print_config_var(tc, part, "v2");
}


ATF_TC_WITH_CLEANUP(print_config);
ATF_TC_HEAD(print_config, tc) {}
ATF_TC_BODY(print_config, tc)
{
    print_config_vars(tc, "body");
}
ATF_TC_CLEANUP(print_config, tc)
{
    print_config_vars(tc, "cleanup");
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, fail);
    ATF_TP_ADD_TC(tp, pass);
    ATF_TP_ADD_TC(tp, signal);
    ATF_TP_ADD_TC(tp, sleep);

    ATF_TP_ADD_TC(tp, cleanup_check_work_directory);
    ATF_TP_ADD_TC(tp, cleanup_fail);
    ATF_TP_ADD_TC(tp, cleanup_signal);
    ATF_TP_ADD_TC(tp, cleanup_sleep);

    ATF_TP_ADD_TC(tp, body_and_cleanup_fail);

    ATF_TP_ADD_TC(tp, print_config);

    return atf_no_error();
}
