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

#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "defs.h"
#include "error.h"
#include "result.h"
#include "run.h"
#include "stacktrace.h"


/// Template for the creation of the temporary work directories.
#define WORKDIR_TEMPLATE "kyua.plain-tester.XXXXXX"


/// Name of the fake test case exposed by the program.
const char* const fake_test_case_name = "main";


/// Converts the exit status of a program to a result.
///
/// \param status Exit status of the test program, as returned by waitpid().
/// \param timed_out Whether the test program timed out or not.
/// \param result_file Path to the result file to create.
/// \param [out] success Set to true if the test program returned with a
///     successful condition.
///
/// \return An error if something went wrong.
static kyua_error_t
status_to_result(int status, const bool timed_out, const char* result_file,
                 bool* success)
{
    if (timed_out) {
        *success = false;
        return kyua_result_write(result_file, KYUA_RESULT_BROKEN,
                                 "Test case timed out");
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *success = true;
            return kyua_result_write(result_file, KYUA_RESULT_PASSED, NULL);
        } else {
            *success = false;
            return kyua_result_write(result_file, KYUA_RESULT_FAILED,
                                     "Returned non-success exit status %d",
                                     WEXITSTATUS(status));
        }
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(result_file, KYUA_RESULT_BROKEN,
                                 "Received signal %d", WTERMSIG(status));
    }
}


/// Lists the test cases in a test program.
///
/// \param unused_test_program Path to the test program for which to list the
///     test cases.  Should be absolute.
/// \param unused_run_params Execution parameters to configure the test process.
///
/// \return An error if the listing fails; OK otherwise.
static kyua_error_t
list_test_cases(const char* KYUA_DEFS_UNUSED_PARAM(test_program),
                const kyua_run_params_t* KYUA_DEFS_UNUSED_PARAM(run_params))
{
    printf("test_case{name='%s'}\n", fake_test_case_name);
    return kyua_error_ok();
}


/// Runs a single test cases of a test program.
///
/// \param test_program Path to the test program for which to list the test
///     cases.  Should be absolute.
/// \param test_case Name of the test case to run.
/// \param result_file Path to the file to which to write the result of the
///     test.  Should be absolute.
/// \param user_variables Array of name=value pairs that describe the user
///     configuration variables for the test case.
/// \param run_params Execution parameters to configure the test process.
/// \param [out] success Set to true if the test case reported a valid exit
///     condition (like "passed" or "skipped"); false otherwise.  This is
///     only updated if the method returns OK.
///
/// \return An error if the listing fails; OK otherwise.
static kyua_error_t
run_test_case(const char* test_program, const char* test_case,
              const char* result_file, const char* const user_variables[],
              const kyua_run_params_t* run_params,
              bool* success)
{
    kyua_error_t error;

    if (strcmp(test_case, fake_test_case_name) != 0) {
        error = kyua_generic_error_new("Unknown test case '%s'", test_case);
        goto out;
    }

    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        warnx("Configuration variables not supported; ignoring '%s'", *iter);
    }

    char *work_directory;
    error = kyua_run_work_directory_enter(WORKDIR_TEMPLATE,
                                          run_params->unprivileged_user,
                                          run_params->unprivileged_group,
                                          &work_directory);
    if (kyua_error_is_set(error))
        goto out;
    kyua_run_params_t real_run_params = *run_params;
    real_run_params.work_directory = work_directory;

    pid_t pid;
    error = kyua_run_fork(&real_run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        const char* const program_args[] = { test_program, NULL };
        kyua_run_exec(test_program, program_args);
    }
    assert(pid != -1 && pid != 0);
    if (kyua_error_is_set(error))
        goto out_work_directory;

    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        goto out_work_directory;

    if (WIFSIGNALED(status) && WCOREDUMP(status)) {
        kyua_stacktrace_dump(test_program, pid, run_params, stderr);
    }

    error = status_to_result(status, timed_out, result_file, success);

out_work_directory:
    error = kyua_error_subsume(error,
        kyua_run_work_directory_leave(&work_directory));
out:
    return error;
}


/// Definition of the tester.
static kyua_cli_tester_t plain_tester = {
    .list_test_cases = list_test_cases,
    .run_test_case = run_test_case,
};


/// Tester entry point.
///
/// \param argc Number of command line arguments.
/// \param argv NULL-terminated array of command line arguments.
///
/// \return An exit code.
int
main(const int argc, char* const* const argv)
{
    return kyua_cli_main(argc, argv, &plain_tester);
}
