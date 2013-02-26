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

#include "cli.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <atf-c.h>

#include "defs.h"
#include "error.h"
#include "run.h"


/// Dumps the contents of a run_params object to stdout.
///
/// We only print the settings that are relevant for testing purposes.
///
/// \param run_params The run parameters to be printed.
static void
dump_run_params(const kyua_run_params_t* run_params)
{
    printf("timeout_seconds: %lu\n", run_params->timeout_seconds);

    if (run_params->unprivileged_user == getuid())
        printf("unprivileged_user: self\n");
    else
        printf("unprivileged_user: %ld\n", (long)run_params->unprivileged_user);

    if (run_params->unprivileged_group == getgid())
        printf("unprivileged_group: self\n");
    else
        printf("unprivileged_group: %ld\n",
               (long)run_params->unprivileged_group);
}


/// Helper to validate argument passing to the list_test_cases method.
///
/// This prints the value of all arguments to stdout so that the caller can
/// compare the printed output to the expected values.
///
/// \param test_program Test program path.
/// \param run_params Execution parameters to configure the test process.
///
/// \return An error if the test_program is set to the magic keyword 'error'; OK
/// otherwise.
static kyua_error_t
list_test_cases(const char* test_program, const kyua_run_params_t* run_params)
{
    if (strcmp(test_program, "error") == 0)
        return kyua_oom_error_new();
    else {
        printf("test_program: %s\n", test_program);
        dump_run_params(run_params);
        return kyua_error_ok();
    }
}


/// Helper to validate argument passing to the run_test_cases method.
///
/// This prints the value of all arguments to stdout so that the caller can
/// compare the printed output to the expected values.
///
/// \param test_program Test program path.
/// \param test_case Test case name.
/// \param result_file Path to the result file.
/// \param user_variables User configuration variables.
/// \param run_params Execution parameters to configure the test process.
/// \param [out] success Whether the test case returned with a successful result
///     or not.  Set to true if result_file is the magic word 'pass'.
///
/// \return An error if the test_program is set to the magic keyword 'error'; OK
/// otherwise.
static kyua_error_t
run_test_case(const char* test_program, const char* test_case,
              const char* result_file, const char* const user_variables[],
              const kyua_run_params_t* run_params, bool* success)
{
    if (strcmp(test_program, "error") == 0)
        return kyua_oom_error_new();
    else {
        printf("test_program: %s\n", test_program);
        printf("test_case: %s\n", test_case);
        printf("result_file: %s\n", result_file);
        const char* const* iter;
        for (iter = user_variables; *iter != NULL; ++iter)
            printf("variable: %s\n", *iter);
        dump_run_params(run_params);
        *success = strcmp(result_file, "pass") == 0;
        return kyua_error_ok();
    }
}


/// Definition of a mock tester.
static kyua_cli_tester_t mock_tester = {
    .list_test_cases = list_test_cases,
    .run_test_case = run_test_case,
};


/// Definition of a tester with invalid values.
///
/// This is to be used when the called code is not supposed to invoke any of the
/// tester methods.
static kyua_cli_tester_t unused_tester = {
    .list_test_cases = NULL,
    .run_test_case = NULL,
};


/// Counts the number of arguments in an argv vector.
///
/// \param argv The NULL-terminated arguments vector to be passed to the
///     kyua_cli_main function.
///
/// \return The number of arguments in argv.
static int
count_argv(char* const* const argv)
{
    int argc = 0;
    char* const* arg;
    for (arg = argv; *arg != NULL; arg++)
        argc++;
    return argc;
}


ATF_TC_WITHOUT_HEAD(main__unknown_option);
ATF_TC_BODY(main__unknown_option, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-Z";
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Unknown option -Z\n");
}


ATF_TC_WITHOUT_HEAD(main__missing_option_argument);
ATF_TC_BODY(main__missing_option_argument, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-t";
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: -t requires an "
                   "argument\n");
}


ATF_TC_WITHOUT_HEAD(main__unknown_command);
ATF_TC_BODY(main__unknown_command, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "foobar";
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Unknown command "
                   "'foobar'\n");
}


ATF_TC_WITHOUT_HEAD(main__missing_command);
ATF_TC_BODY(main__missing_command, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char* const argv[] = {arg0, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Must provide a "
                   "command\n");
}


/// Checks that a textual argument to a numerical flag raises an error.
///
/// \param flag The generic flag to test.
/// \param error_message The expected error message when the flag is invalid.
static void
check_flag_not_a_number(const char flag, const char *error_message)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-?foo";
        arg1[1] = flag;
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    char experr[256];
    snprintf(experr, sizeof(experr), "cli_test: %s 'foo' (not a number)\n",
             error_message);
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", experr);
}


/// Checks that an out of range value to a numerical flag raises an error.
///
/// \param flag The generic flag to test.
/// \param error_message The expected error message when the flag is invalid.
static void
check_flag_out_of_range(const char flag, const char *error_message)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-?99999999999999999999";
        arg1[1] = flag;
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    char experr[256];
    snprintf(experr, sizeof(experr), "cli_test: %s '99999999999999999999' "
             "(out of range)\n", error_message);
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", experr);
}


ATF_TC_WITHOUT_HEAD(main__gflag__not_a_number);
ATF_TC_BODY(main__gflag__not_a_number, tc)
{
    check_flag_not_a_number('g', "Invalid GID");
}


ATF_TC_WITHOUT_HEAD(main__gflag__out_of_range);
ATF_TC_BODY(main__gflag__out_of_range, tc)
{
    check_flag_out_of_range('g', "Invalid GID");
}


ATF_TC_WITHOUT_HEAD(main__tflag__not_a_number);
ATF_TC_BODY(main__tflag__not_a_number, tc)
{
    check_flag_not_a_number('t', "Invalid timeout value");
}


ATF_TC_WITHOUT_HEAD(main__tflag__out_of_range);
ATF_TC_BODY(main__tflag__out_of_range, tc)
{
    check_flag_out_of_range('t', "Invalid timeout value");
}


ATF_TC_WITHOUT_HEAD(main__uflag__not_a_number);
ATF_TC_BODY(main__uflag__not_a_number, tc)
{
    check_flag_not_a_number('u', "Invalid UID");
}


ATF_TC_WITHOUT_HEAD(main__uflag__out_of_range);
ATF_TC_BODY(main__uflag__out_of_range, tc)
{
    check_flag_out_of_range('u', "Invalid UID");
}


ATF_TC_WITHOUT_HEAD(list__ok);
ATF_TC_BODY(list__ok, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "list";
        char arg2[] = "the-program";
        char* const argv[] = {arg0, arg1, arg2, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_SUCCESS,
                   "test_program: the-program\n"
                   "timeout_seconds: 60\n"
                   "unprivileged_user: self\n"
                   "unprivileged_group: self\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(list__custom_run_params);
ATF_TC_BODY(list__custom_run_params, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-g987";
        char arg2[] = "-t123";
        char arg3[] = "-u45";
        char arg4[] = "list";
        char arg5[] = "the-program";
        char* const argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_SUCCESS,
                   "test_program: the-program\n"
                   "timeout_seconds: 123\n"
                   "unprivileged_user: 45\n"
                   "unprivileged_group: 987\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(list__error);
ATF_TC_BODY(list__error, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "list";
        char arg2[] = "error";
        char* const argv[] = {arg0, arg1, arg2, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_INTERNAL_ERROR, "", "cli_test: Not enough "
                   "memory\n");
}


ATF_TC_WITHOUT_HEAD(list__missing_arguments);
ATF_TC_BODY(list__missing_arguments, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "list";
        char* const argv[] = {arg0, arg1, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: No test program "
                   "provided\n");
}


ATF_TC_WITHOUT_HEAD(list__too_many_arguments);
ATF_TC_BODY(list__too_many_arguments, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "list";
        char arg2[] = "first";
        char arg3[] = "second";
        char* const argv[] = {arg0, arg1, arg2, arg3, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Only one test program "
                   "allowed\n");
}


ATF_TC_WITHOUT_HEAD(test__ok__pass);
ATF_TC_BODY(test__ok__pass, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "the-program";
        char arg3[] = "the-test-case";
        char arg4[] = "pass";
        char* const argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_SUCCESS,
                   "test_program: the-program\n"
                   "test_case: the-test-case\n"
                   "result_file: pass\n"
                   "timeout_seconds: 60\n"
                   "unprivileged_user: self\n"
                   "unprivileged_group: self\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(test__ok__fail);
ATF_TC_BODY(test__ok__fail, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "the-program";
        char arg3[] = "the-test-case";
        char arg4[] = "fail";
        char* const argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_FAILURE,
                   "test_program: the-program\n"
                   "test_case: the-test-case\n"
                   "result_file: fail\n"
                   "timeout_seconds: 60\n"
                   "unprivileged_user: self\n"
                   "unprivileged_group: self\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(test__custom_run_params);
ATF_TC_BODY(test__custom_run_params, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "-g987";
        char arg2[] = "-t123";
        char arg3[] = "-u45";
        char arg4[] = "test";
        char arg5[] = "the-program";
        char arg6[] = "the-test-case";
        char arg7[] = "pass";
        char* const argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                              NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_SUCCESS,
                   "test_program: the-program\n"
                   "test_case: the-test-case\n"
                   "result_file: pass\n"
                   "timeout_seconds: 123\n"
                   "unprivileged_user: 45\n"
                   "unprivileged_group: 987\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(test__config_variables);
ATF_TC_BODY(test__config_variables, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "-vfoo=bar";
        char arg3[] = "-va=c";
        char arg4[] = "the-program";
        char arg5[] = "the-test-case";
        char arg6[] = "pass";
        char* const argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_SUCCESS,
                   "test_program: the-program\n"
                   "test_case: the-test-case\n"
                   "result_file: pass\n"
                   "variable: foo=bar\n"
                   "variable: a=c\n"
                   "timeout_seconds: 60\n"
                   "unprivileged_user: self\n"
                   "unprivileged_group: self\n",
                   "");
}


ATF_TC_WITHOUT_HEAD(test__error);
ATF_TC_BODY(test__error, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "error";
        char* const argv[] = {arg0, arg1, arg2, arg2, arg2, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &mock_tester));
    }
    atf_utils_wait(pid, EXIT_INTERNAL_ERROR, "", "cli_test: Not enough "
                   "memory\n");
}


/// Checks that the test command validates the right number of arguments.
///
/// \param count Number of arguments to pass to the test command.
static void
check_test_invalid_arguments(const unsigned int count)
{
    printf("Checking with %d arguments\n", count);
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char argX[] = "arg";
        assert(count <= 4);
        char* argv[] = {arg0, arg1, argX, argX, argX, argX, NULL};
        argv[2 + count] = NULL;
        exit(kyua_cli_main(2 + count, argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Must provide a test "
                   "program, a test case name and a result file\n");
}


ATF_TC_WITHOUT_HEAD(test__invalid_arguments);
ATF_TC_BODY(test__invalid_arguments, tc)
{
    check_test_invalid_arguments(0);
    check_test_invalid_arguments(1);
    check_test_invalid_arguments(2);
    check_test_invalid_arguments(4);
}


ATF_TC_WITHOUT_HEAD(test__unknown_option);
ATF_TC_BODY(test__unknown_option, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "-Z";
        char* const argv[] = {arg0, arg1, arg2, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: Unknown test option "
                   "-Z\n");
}


ATF_TC_WITHOUT_HEAD(test__missing_option_argument);
ATF_TC_BODY(test__missing_option_argument, tc)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        char arg0[] = "unused-progname";
        char arg1[] = "test";
        char arg2[] = "-v";
        char* const argv[] = {arg0, arg1, arg2, NULL};
        exit(kyua_cli_main(count_argv(argv), argv, &unused_tester));
    }
    atf_utils_wait(pid, EXIT_USAGE_ERROR, "", "cli_test: test's -v requires an "
                   "argument\n");
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, main__unknown_option);
    ATF_TP_ADD_TC(tp, main__missing_option_argument);
    ATF_TP_ADD_TC(tp, main__unknown_command);
    ATF_TP_ADD_TC(tp, main__missing_command);
    ATF_TP_ADD_TC(tp, main__gflag__not_a_number);
    ATF_TP_ADD_TC(tp, main__gflag__out_of_range);
    ATF_TP_ADD_TC(tp, main__tflag__not_a_number);
    ATF_TP_ADD_TC(tp, main__tflag__out_of_range);
    ATF_TP_ADD_TC(tp, main__uflag__not_a_number);
    ATF_TP_ADD_TC(tp, main__uflag__out_of_range);

    ATF_TP_ADD_TC(tp, list__ok);
    ATF_TP_ADD_TC(tp, list__custom_run_params);
    ATF_TP_ADD_TC(tp, list__error);
    ATF_TP_ADD_TC(tp, list__missing_arguments);
    ATF_TP_ADD_TC(tp, list__too_many_arguments);

    ATF_TP_ADD_TC(tp, test__ok__pass);
    ATF_TP_ADD_TC(tp, test__ok__fail);
    ATF_TP_ADD_TC(tp, test__custom_run_params);
    ATF_TP_ADD_TC(tp, test__config_variables);
    ATF_TP_ADD_TC(tp, test__error);
    ATF_TP_ADD_TC(tp, test__invalid_arguments);
    ATF_TP_ADD_TC(tp, test__unknown_option);
    ATF_TP_ADD_TC(tp, test__missing_option_argument);

    return atf_no_error();
}
