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

#include "atf_result.h"

#include <sys/resource.h>
#include <sys/wait.h>

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#include "error.h"


/// Evalutes an expression and ensures it does not return an error.
///
/// \param expr A expression that must evaluate to kyua_error_t.
#define RE(expr) ATF_REQUIRE(!kyua_error_is_set(expr))


/// Generates a wait(2) status for a successful exit code.
///
/// \param exitstatus The exit status to encode in the status.
///
/// \return The wait(2) status.
static int
generate_wait_exitstatus(const int exitstatus)
{
    const pid_t pid = fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        exit(exitstatus);
    } else {
        int status;
        ATF_REQUIRE(waitpid(pid, &status, 0) != -1);
        return status;
    }
}


/// Generates a wait(2) status for a termination due to a signal.
///
/// \param signo The signal number to encode in the status.
///
/// \return The wait(2) status.
static int
generate_wait_termsig(const int signo)
{
    // Explicitly disable core files to avoid inconsistent behaviors across
    // operating systems.  Some of the signal numbers passed to this function
    // may have a meaning or not depending on the system, and this might mean
    // that a core gets generated arbitrarily.  As a result of this, our string
    // comparisons below fail.
    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) == -1)
        atf_tc_skip("Failed to lower the core size limit");

    const pid_t pid = fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        kill(getpid(), signo);
        abort();
    } else {
        int status;
        ATF_REQUIRE(waitpid(pid, &status, 0) != -1);
        return status;
    }
}


/// Validates an execution of kyua_atf_result_rewrite().
///
/// The code of this check is all within the macro instead of in a separate
/// function so that we get meaningful line numbers in test failure messages.
///
/// \param name Name of the test case.
/// \param input Text of the ATF result.
/// \param wait_status Exit status of the ATF test case, as returned by wait(2).
/// \param timed_out Whether the test case timed out or not.
/// \param exp_output Text of the expected generic result.
/// \param exp_success Expected value of the success result.
#define CHECK(name, input, wait_status, timed_out, exp_output, exp_success) \
    ATF_TC_WITHOUT_HEAD(name); \
    ATF_TC_BODY(name, tc) \
    { \
        bool success; \
        atf_utils_create_file("in.txt", "%s", input); \
        RE(kyua_atf_result_rewrite("in.txt", "out.txt", wait_status, \
                                   timed_out, &success)); \
        atf_utils_cat_file("out.txt", "OUTPUT: "); \
        ATF_REQUIRE(atf_utils_compare_file("out.txt", exp_output)); \
        ATF_REQUIRE_EQ(exp_success, success); \
    }


CHECK(rewrite__expected_death__exit_failure,
      "expected_death: Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_death__signaled,
      "expected_death: Foo bar\n",
      generate_wait_termsig(1), false,
      "expected_failure: Foo bar\n",
      true);
CHECK(rewrite__expected_death__exit_success,
      "expected_death: Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "failed: Test case expected to die but exited successfully\n",
      false);
CHECK(rewrite__expected_death__no_reason,
      "expected_death\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 1\n",
      false);
CHECK(rewrite__expected_death__unexpected_arg,
      "expected_death(23): Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Unknown test case result status expected_death(23); "
      "test case exited with code 1\n",
      false);


CHECK(rewrite__expected_exit__exit_success,
      "expected_exit: Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_exit__exit_failure,
      "expected_exit: Some other text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "expected_failure: Some other text\n",
      true);
CHECK(rewrite__expected_exit__arg_match,
      "expected_exit(15): Some text\n",
      generate_wait_exitstatus(15), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_exit__arg_not_match,
      "expected_exit(18): Some text\n",
      generate_wait_exitstatus(15), false,
      "failed: Test case expected to exit with code 18 but got code 15\n",
      false);
CHECK(rewrite__expected_exit__signaled,
      "expected_exit: Foo bar\n",
      generate_wait_termsig(1), false,
      "failed: Test case expected to exit normally but received signal 1\n",
      false);
CHECK(rewrite__expected_exit__no_reason,
      "expected_exit\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 1\n",
      false);
CHECK(rewrite__expected_exit__bad_arg,
      "expected_exit(abc): Some text\n",
      generate_wait_exitstatus(25), false,
      "broken: Invalid status argument (abc): not a number; test case exited "
      "with code 25\n",
      false);


CHECK(rewrite__expected_failure__ok,
      "expected_failure: Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_failure__failed,
      "expected_failure: Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "failed: Test case expected a failure but exited with error code 1\n",
      false);
CHECK(rewrite__expected_failure__signaled,
      "expected_failure: Foo bar\n",
      generate_wait_termsig(1), false,
      "failed: Test case expected a failure but received signal 1\n",
      false);
CHECK(rewrite__expected_failure__no_reason,
      "expected_failure\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 1\n",
      false);
CHECK(rewrite__expected_failure__unexpected_arg,
      "expected_failure(23): Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Unknown test case result status expected_failure(23); "
      "test case exited with code 1\n",
      false);


CHECK(rewrite__expected_signal__ok,
      "expected_signal: Some text\n",
      generate_wait_termsig(6), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_signal__arg_match,
      "expected_signal(15): Some text\n",
      generate_wait_termsig(15), false,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_signal__arg_not_match,
      "expected_signal(18): Some text\n",
      generate_wait_termsig(15), false,
      "failed: Test case expected to receive signal 18 but got 15\n",
      false);
CHECK(rewrite__expected_signal__exited,
      "expected_signal: Foo bar\n",
      generate_wait_exitstatus(12), false,
      "failed: Test case expected to receive a signal but exited with code 12\n",
      false);
CHECK(rewrite__expected_signal__no_reason,
      "expected_signal\n",
      generate_wait_termsig(15), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case received signal 15\n",
      false);
CHECK(rewrite__expected_signal__bad_arg,
      "expected_signal(abc): Some text\n",
      generate_wait_termsig(25), false,
      "broken: Invalid status argument (abc): not a number; test case received "
      "signal 25\n",
      false);


CHECK(rewrite__expected_timeout__ok,
      "expected_timeout: Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), true,
      "expected_failure: Some text\n",
      true);
CHECK(rewrite__expected_timeout__exited,
      "expected_timeout: Foo bar\n",
      generate_wait_exitstatus(12), false,
      "failed: Test case expected to time out but exited with code 12\n",
      false);
CHECK(rewrite__expected_timeout__signaled,
      "expected_timeout: Foo bar\n",
      generate_wait_termsig(15), false,
      "failed: Test case expected to time out but received signal 15\n",
      false);
CHECK(rewrite__expected_timeout__no_reason,
      "expected_timeout\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 1\n",
      false);
CHECK(rewrite__expected_timeout__unexpected_arg,
      "expected_timeout(23): Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Unknown test case result status expected_timeout(23); "
      "test case exited with code 1\n",
      false);


CHECK(rewrite__failed__ok,
      "failed: Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "failed: Some text\n",
      false);
CHECK(rewrite__failed__exit_success,
      "failed: Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "broken: Test case reported a failed result but exited with a successful "
      "exit code\n",
      false);
CHECK(rewrite__failed__signaled,
      "failed: Not used\n",
      generate_wait_termsig(1), false,
      "broken: Test case reported a failed result but received signal 1\n",
      false);
CHECK(rewrite__failed__no_reason,
      "failed\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 1\n",
      false);
CHECK(rewrite__failed__unexpected_arg,
      "failed(23): Some text\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Unknown test case result status failed(23); "
      "test case exited with code 1\n",
      false);


CHECK(rewrite__passed__ok,
      "passed\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "passed\n",
      true);
CHECK(rewrite__passed__exit_failure,
      "passed\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case reported a passed result but returned a non-zero "
      "exit code 1\n",
      false);
CHECK(rewrite__passed__signaled,
      "passed\n",
      generate_wait_termsig(1), false,
      "broken: Test case reported a passed result but received signal 1\n",
      false);
CHECK(rewrite__passed__unexpected_reason,
      "passed: This should not be here\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "broken: Found unexpected reason in passed test result; test case exited "
      "with code 0\n",
      false);
CHECK(rewrite__passed__unexpected_arg,
      "passed(0)\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "broken: Unknown test case result status passed(0); "
      "test case exited with code 0\n",
      false);


CHECK(rewrite__skipped__ok,
      "skipped: Does not apply\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "skipped: Does not apply\n",
      true);
CHECK(rewrite__skipped__exit_failure,
      "skipped: Some reason\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Test case reported a skipped result but returned a non-zero "
      "exit code 1\n",
      false);
CHECK(rewrite__skipped__signaled,
      "skipped: Not used\n",
      generate_wait_termsig(1), false,
      "broken: Test case reported a skipped result but received signal 1\n",
      false);
CHECK(rewrite__skipped__no_reason,
      "skipped\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "broken: Test case should have reported a failure reason but didn't; "
      "test case exited with code 0\n",
      false);
CHECK(rewrite__skipped__unexpected_arg,
      "skipped(1): Some text\n",
      generate_wait_exitstatus(EXIT_SUCCESS), false,
      "broken: Unknown test case result status skipped(1); "
      "test case exited with code 0\n",
      false);


CHECK(rewrite__timed_out,
      "this invalid result file should not be parsed\n",
      generate_wait_exitstatus(EXIT_SUCCESS), true,
      "broken: Test case body timed out\n",
      false);


ATF_TC_WITHOUT_HEAD(rewrite__missing_file);
ATF_TC_BODY(rewrite__missing_file, tc)
{
    bool success;
    RE(kyua_atf_result_rewrite("in.txt", "out.txt",
                               generate_wait_exitstatus(EXIT_SUCCESS),
                               false, &success));
    atf_utils_cat_file("out.txt", "OUTPUT: ");
    ATF_REQUIRE(atf_utils_compare_file("out.txt",
        "broken: Premature exit; test case exited with code 0\n"));
    ATF_REQUIRE(!success);
}
CHECK(rewrite__empty,
      "",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "broken: Empty result file in.txt; test case exited with code 1\n",
      false);
CHECK(rewrite__unknown_status,
      "foo\n",
      generate_wait_exitstatus(123), false,
      "broken: Unknown test case result status foo; test case exited with "
      "code 123\n",
      false);
CHECK(rewrite__status_without_newline,
      "failed",
      generate_wait_termsig(1), false,
      "broken: Missing newline in result file; test case received signal 1\n",
      false);
CHECK(rewrite__status_multiline,
      "failed: First line\nSecond line\nThird line",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "failed: First line<<NEWLINE>>Second line<<NEWLINE>>Third line\n",
      false);
CHECK(rewrite__status_multiline_2,
      "failed: First line\nSecond line\nThird line\n",
      generate_wait_exitstatus(EXIT_FAILURE), false,
      "failed: First line<<NEWLINE>>Second line<<NEWLINE>>Third line\n",
      false);


/// Validates an execution of kyua_atf_result_cleanup_rewrite().
///
/// The code of this check is all within the macro instead of in a separate
/// function so that we get meaningful line numbers in test failure messages.
///
/// \param name Name of the test case.
/// \param wait_status Exit status of the ATF test case, as returned by wait(2).
/// \param timed_out Whether the test case timed out or not.
/// \param exp_output Text of the expected generic result.
/// \param exp_success Expected value of the success result.
#define CHECK_CLEANUP(name, wait_status, timed_out, exp_output, exp_success) \
    ATF_TC_WITHOUT_HEAD(name); \
    ATF_TC_BODY(name, tc) \
    { \
        bool success; \
        atf_utils_create_file("out.txt", "skipped: Preexistent file\n"); \
        RE(kyua_atf_result_cleanup_rewrite("out.txt", wait_status, \
                                           timed_out, &success)); \
        atf_utils_cat_file("out.txt", "OUTPUT: "); \
        ATF_REQUIRE(atf_utils_compare_file("out.txt", exp_output)); \
        ATF_REQUIRE_EQ(exp_success, success); \
    }


CHECK_CLEANUP(cleanup_rewrite__ok,
              generate_wait_exitstatus(EXIT_SUCCESS), false,
              "skipped: Preexistent file\n",  // Previous file not overwritten.
              true);
CHECK_CLEANUP(cleanup_rewrite__exit_failure,
              generate_wait_exitstatus(EXIT_FAILURE), false,
              "broken: Test case cleanup exited with code 1\n",
              false);
CHECK_CLEANUP(cleanup_rewrite__signaled,
              generate_wait_termsig(1), false,
              "broken: Test case cleanup received signal 1\n",
              false);
CHECK_CLEANUP(cleanup_rewrite__timed_out,
              generate_wait_exitstatus(EXIT_SUCCESS), true,
              "broken: Test case cleanup timed out\n",
              false);


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, rewrite__expected_death__exit_failure);
    ATF_TP_ADD_TC(tp, rewrite__expected_death__signaled);
    ATF_TP_ADD_TC(tp, rewrite__expected_death__exit_success);
    ATF_TP_ADD_TC(tp, rewrite__expected_death__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__expected_death__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__expected_exit__exit_success);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__exit_failure);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__arg_match);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__arg_not_match);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__signaled);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__expected_exit__bad_arg);

    ATF_TP_ADD_TC(tp, rewrite__expected_failure__ok);
    ATF_TP_ADD_TC(tp, rewrite__expected_failure__failed);
    ATF_TP_ADD_TC(tp, rewrite__expected_failure__signaled);
    ATF_TP_ADD_TC(tp, rewrite__expected_failure__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__expected_failure__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__expected_signal__ok);
    ATF_TP_ADD_TC(tp, rewrite__expected_signal__arg_match);
    ATF_TP_ADD_TC(tp, rewrite__expected_signal__arg_not_match);
    ATF_TP_ADD_TC(tp, rewrite__expected_signal__exited);
    ATF_TP_ADD_TC(tp, rewrite__expected_signal__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__expected_signal__bad_arg);

    ATF_TP_ADD_TC(tp, rewrite__expected_timeout__ok);
    ATF_TP_ADD_TC(tp, rewrite__expected_timeout__exited);
    ATF_TP_ADD_TC(tp, rewrite__expected_timeout__signaled);
    ATF_TP_ADD_TC(tp, rewrite__expected_timeout__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__expected_timeout__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__failed__ok);
    ATF_TP_ADD_TC(tp, rewrite__failed__exit_success);
    ATF_TP_ADD_TC(tp, rewrite__failed__signaled);
    ATF_TP_ADD_TC(tp, rewrite__failed__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__failed__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__passed__ok);
    ATF_TP_ADD_TC(tp, rewrite__passed__exit_failure);
    ATF_TP_ADD_TC(tp, rewrite__passed__signaled);
    ATF_TP_ADD_TC(tp, rewrite__passed__unexpected_reason);
    ATF_TP_ADD_TC(tp, rewrite__passed__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__skipped__ok);
    ATF_TP_ADD_TC(tp, rewrite__skipped__exit_failure);
    ATF_TP_ADD_TC(tp, rewrite__skipped__signaled);
    ATF_TP_ADD_TC(tp, rewrite__skipped__no_reason);
    ATF_TP_ADD_TC(tp, rewrite__skipped__unexpected_arg);

    ATF_TP_ADD_TC(tp, rewrite__timed_out);

    ATF_TP_ADD_TC(tp, rewrite__missing_file);
    ATF_TP_ADD_TC(tp, rewrite__empty);
    ATF_TP_ADD_TC(tp, rewrite__unknown_status);
    ATF_TP_ADD_TC(tp, rewrite__status_without_newline);
    ATF_TP_ADD_TC(tp, rewrite__status_multiline);
    ATF_TP_ADD_TC(tp, rewrite__status_multiline_2);

    ATF_TP_ADD_TC(tp, cleanup_rewrite__ok);
    ATF_TP_ADD_TC(tp, cleanup_rewrite__exit_failure);
    ATF_TP_ADD_TC(tp, cleanup_rewrite__signaled);
    ATF_TP_ADD_TC(tp, cleanup_rewrite__timed_out);

    return atf_no_error();
}
