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

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "result.h"

#if !defined(WCOREDUMP) && defined(__minix)
#define WCOREDUMP(x) ((x) & 0x80)
#endif /*!defined(WCOREDUMP) && defined(__minix) */


// Enumeration of the different result types returned by an ATF test case.
enum atf_status {
    ATF_STATUS_EXPECTED_DEATH,
    ATF_STATUS_EXPECTED_EXIT,
    ATF_STATUS_EXPECTED_FAILURE,
    ATF_STATUS_EXPECTED_SIGNAL,
    ATF_STATUS_EXPECTED_TIMEOUT,
    ATF_STATUS_FAILED,
    ATF_STATUS_PASSED,
    ATF_STATUS_SKIPPED,

    // The broken status below is never returned by the test cases themselves.
    // We use it internally to pass around problems detected while dealing with
    // the test case itself (like an invalid result file).
    ATF_STATUS_BROKEN,
};


/// Magic number representing a missing argument to the test result status.
///
/// Use this to specify that an expected_exit or expected_signal result accepts
/// any exit code or signal, respectively.
#define NO_STATUS_ARG -1


/// Removes a trailing newline from a string (supposedly read by fgets(3)).
///
/// \param [in,out] str The string to remove the trailing newline from.
///
/// \return True if there was a newline character; false otherwise.
static bool
trim_newline(char* str)
{
    const size_t length = strlen(str);
    if (length == 0) {
        return false;
    } else {
        if (str[length - 1] == '\n') {
            str[length - 1] = '\0';
            return true;
        } else {
            return false;
        }
    }
}


/// Force read on stream to see if we are really at EOF.
///
/// A call to fgets(3) will not return EOF when it returns valid data.  But
/// because of our semantics below, we need to be able to tell if more lines are
/// available before actually reading them.
///
/// \param input The stream to check for EOF.
///
/// \return True if the stream is not at EOF yet; false otherwise.
static bool
is_really_eof(FILE* input)
{
    const int ch = getc(input);
    const bool real_eof = feof(input);
    (void)ungetc(ch, input);
    return real_eof;
}


/// Parses the optional argument to a result status.
///
/// \param str Pointer to the argument.  May be \0 in those cases where the
///     status does not have any argument.
/// \param [out] status_arg Value of the parsed argument.
///
/// \return OK if the argument exists and is valid, or if it does not exist; an
/// error otherwise.
static kyua_error_t
parse_status_arg(const char* str, int* status_arg)
{
    if (*str == '\0') {
        *status_arg = NO_STATUS_ARG;
        return kyua_error_ok();
    }

    const size_t length = strlen(str);
    if (*str != '(' || *(str + length - 1) != ')')
        return kyua_generic_error_new("Invalid status argument %s", str);
    const char* const arg = str + 1;

    char* endptr;
    const long value = strtol(arg, &endptr, 10);
    if (arg[0] == '\0' || endptr != str + length - 1)
        return kyua_generic_error_new("Invalid status argument %s: not a "
                                      "number", str);
    if (errno == ERANGE && (value == LONG_MAX || value == LONG_MIN))
        return kyua_generic_error_new("Invalid status argument %s: out of "
                                      "range", str);
    if (value < INT_MIN || value > INT_MAX)
        return kyua_generic_error_new("Invalid status argument %s: out of "
                                      "range", str);

    *status_arg = (int)value;
    return kyua_error_ok();
}


/// Parses a textual result status.
///
/// \param str The text to parse.
/// \param [out] status Status type if the input is valid.
/// \param [out] status_arg Optional integral argument to the status.
/// \param [out] need_reason Whether the detected status requires a reason.
///
/// \return An error if the status is not valid.
static kyua_error_t
parse_status(const char* str, enum atf_status* status, int* status_arg,
             bool* need_reason)
{
    if (strcmp(str, "passed") == 0) {
        *status = ATF_STATUS_PASSED;
        *need_reason = false;
        return kyua_error_ok();
    } else if (strcmp(str, "failed") == 0) {
        *status = ATF_STATUS_FAILED;
        *need_reason = true;
        return kyua_error_ok();
    } else if (strcmp(str, "skipped") == 0) {
        *status = ATF_STATUS_SKIPPED;
        *need_reason = true;
        return kyua_error_ok();
    } else if (strcmp(str, "expected_death") == 0) {
        *status = ATF_STATUS_EXPECTED_DEATH;
        *need_reason = true;
        return kyua_error_ok();
    } else if (strncmp(str, "expected_exit", 13) == 0) {
        *status = ATF_STATUS_EXPECTED_EXIT;
        *need_reason = true;
        return parse_status_arg(str + 13, status_arg);
    } else if (strcmp(str, "expected_failure") == 0) {
        *status = ATF_STATUS_EXPECTED_FAILURE;
        *need_reason = true;
        return kyua_error_ok();
    } else if (strncmp(str, "expected_signal", 15) == 0){
        *status = ATF_STATUS_EXPECTED_SIGNAL;
        *need_reason = true;
        return parse_status_arg(str + 15, status_arg);
    } else if (strcmp(str, "expected_timeout") == 0) {
        *status = ATF_STATUS_EXPECTED_TIMEOUT;
        *need_reason = true;
        return kyua_error_ok();
    } else {
        return kyua_generic_error_new("Unknown test case result status %s",
                                      str);
    }
}


/// Advances a pointer to a buffer to its end.
///
/// \param [in,out] buffer Current buffer contents; updated on exit to point to
///     the termination character.
/// \param [in,out] buffer_size Current buffer size; updated on exit to account
///     for the decreased capacity due to the pointer increase.
static void
advance(char** buffer, size_t* buffer_size)
{
    const size_t increment = strlen(*buffer);
    *buffer += increment;
    *buffer_size -= increment;
}


/// Extracts the result reason from the input file.
///
/// \pre This can only be called for those result types that require a reason.
///
/// \param [in,out] input The file from which to read.
/// \param first_line The first line of the reason.  Because this is part of the
///     same line in which the result status is printed, this line has already
///     been read by the caller and thus must be provided here.
/// \param [out] output Buffer to which to write the full reason.
/// \param output_size Size of the output buffer.
///
/// \return An error if there was no reason in the input or if there is a
/// problem reading it.
static kyua_error_t
read_reason(FILE* input, const char* first_line, char* output,
            size_t output_size)
{
    if (first_line == NULL || *first_line == '\0')
        return kyua_generic_error_new("Test case should have reported a "
                                      "failure reason but didn't");

    snprintf(output, output_size, "%s", first_line);
    advance(&output, &output_size);

    bool had_newline = true;
    while (!is_really_eof(input)) {
        if (had_newline) {
            snprintf(output, output_size, "<<NEWLINE>>");
            advance(&output, &output_size);
        }

        if (fgets(output, output_size, input) == NULL) {
            assert(ferror(input));
            return kyua_libc_error_new(errno, "Failed to read reason from "
                                       "result file");
        }
        had_newline = trim_newline(output);
        advance(&output, &output_size);
    }

    return kyua_error_ok();
}


/// Parses a results file written by an ATF test case.
///
/// \param input_name Path to the result file to parse.
/// \param [out] status Type of result.
/// \param [out] status_arg Optional integral argument to the status.
/// \param [out] reason Textual explanation of the result, if any.
/// \param reason_size Length of the reason output buffer.
///
/// \return An error if the input_name file has an invalid syntax; OK otherwise.
static kyua_error_t
read_atf_result(const char* input_name, enum atf_status* status,
                int* status_arg, char* const reason, const size_t reason_size)
{
    kyua_error_t error = kyua_error_ok();

    FILE* input = fopen(input_name, "r");
    if (input == NULL) {
        error = kyua_generic_error_new("Premature exit");
        goto out;
    }

    char line[1024];
    if (fgets(line, sizeof(line), input) == NULL) {
        if (ferror(input)) {
            error = kyua_libc_error_new(errno, "Failed to read result from "
                                        "file %s", input_name);
            goto out_input;
        } else {
            assert(feof(input));
            error = kyua_generic_error_new("Empty result file %s", input_name);
            goto out_input;
        }
    }

    if (!trim_newline(line)) {
        error = kyua_generic_error_new("Missing newline in result file");
        goto out_input;
    }

    char* reason_start = strstr(line, ": ");
    if (reason_start != NULL) {
        *reason_start = '\0';
        *(reason_start + 1) = '\0';
        reason_start += 2;
    }

    bool need_reason = false;  // Initialize to shut up gcc warning.
    error = parse_status(line, status, status_arg, &need_reason);
    if (kyua_error_is_set(error))
        goto out_input;

    if (need_reason) {
        error = read_reason(input, reason_start, reason, reason_size);
    } else {
        if (reason_start != NULL || !is_really_eof(input)) {
            error = kyua_generic_error_new("Found unexpected reason in passed "
                                           "test result");
            goto out_input;
        }
        reason[0] = '\0';
    }

out_input:
    fclose(input);
out:
    return error;
}


/// Writes a generic result file for an ATF broken result.
///
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_broken(const char* reason, int status, const char* output,
               bool* success)
{
    if (WIFEXITED(status)) {
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_BROKEN, "%s; test case exited with code %d",
            reason, WEXITSTATUS(status));
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_BROKEN, "%s; test case received signal %d%s",
            reason, WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF expected_death result.
///
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_expected_death(const char* reason, int status, const char* output,
                       bool* success)
{
    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected to die but exited "
            "successfully");
    } else {
        *success = true;
        return kyua_result_write(
            output, KYUA_RESULT_EXPECTED_FAILURE, "%s", reason);
    }
}


/// Writes a generic result file for an ATF expected_exit result
///
/// \param status_arg Optional integral argument to the status.
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_expected_exit(const int status_arg, const char* reason, int status,
                      const char* output, bool* success)
{
    if (WIFEXITED(status)) {
        if (status_arg == NO_STATUS_ARG || status_arg == WEXITSTATUS(status)) {
            *success = true;
            return kyua_result_write(
                output, KYUA_RESULT_EXPECTED_FAILURE, "%s", reason);
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_FAILED, "Test case expected to exit with "
                "code %d but got code %d", status_arg, WEXITSTATUS(status));
        }
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected to exit normally "
            "but received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF expected_failure result.
///
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_expected_failure(const char* reason, int status, const char* output,
                         bool* success)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *success = true;
            return kyua_result_write(
                output, KYUA_RESULT_EXPECTED_FAILURE, "%s", reason);
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_FAILED, "Test case expected a failure but "
                "exited with error code %d", WEXITSTATUS(status));
        }
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected a failure but "
            "received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF expected_signal result.
///
/// \param status_arg Optional integral argument to the status.
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_expected_signal(const int status_arg, const char* reason, int status,
                        const char* output, bool* success)
{
    if (WIFSIGNALED(status)) {
        if (status_arg == NO_STATUS_ARG || status_arg == WTERMSIG(status)) {
            *success = true;
            return kyua_result_write(
                output, KYUA_RESULT_EXPECTED_FAILURE, "%s", reason);
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_FAILED, "Test case expected to receive "
                "signal %d but got %d", status_arg, WTERMSIG(status));
        }
    } else {
        assert(WIFEXITED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected to receive a "
            "signal but exited with code %d", WEXITSTATUS(status));
    }
}


/// Writes a generic result file for an ATF expected_timeout result.
///
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_expected_timeout(int status, const char* output, bool* success)
{
    if (WIFEXITED(status)) {
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected to time out but "
            "exited with code %d", WEXITSTATUS(status));
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_FAILED, "Test case expected to time out but "
            "received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF failed result.
///
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_failed(const char* reason, int status, const char* output,
               bool* success)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_BROKEN, "Test case reported a failed "
                "result but exited with a successful exit code");
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_FAILED, "%s", reason);
        }
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_BROKEN, "Test case reported a failed result "
            "but received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF passed result.
///
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_passed(int status, const char* output, bool* success)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *success = true;
            return kyua_result_write(output, KYUA_RESULT_PASSED, NULL);
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_BROKEN, "Test case reported a passed "
                "result but returned a non-zero exit code %d",
                WEXITSTATUS(status));
        }
    } else {
        assert(WIFSIGNALED(status));
        *success = false;
        return kyua_result_write(
            output, KYUA_RESULT_BROKEN, "Test case reported a passed result "
            "but received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file for an ATF skipped result.
///
/// \param reason Textual explanation of the result.
/// \param status Exit code of the test program as returned by wait().
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_skipped(const char* reason, int status, const char* output,
                bool* success)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *success = true;
            return kyua_result_write(output, KYUA_RESULT_SKIPPED, "%s", reason);
        } else {
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_BROKEN, "Test case reported a skipped "
                "result but returned a non-zero exit code %d",
                WEXITSTATUS(status));
        }
    } else {
        *success = false;
        assert(WIFSIGNALED(status));
        return kyua_result_write(
            output, KYUA_RESULT_BROKEN, "Test case reported a skipped result "
            "but received signal %d%s", WTERMSIG(status),
            WCOREDUMP(status) ? " (core dumped)" : "");
    }
}


/// Writes a generic result file based on an ATF result and an exit code.
///
/// \param status Type of the ATF result.
/// \param status_arg Optional integral argument to the status.
/// \param reason Textual explanation of the result.
/// \param wait_status Exit code of the test program as returned by wait().
/// \param timed_out Whether the test program timed out or not.
/// \param output Path to the generic result file to create.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
static kyua_error_t
convert_result(const enum atf_status status, const int status_arg,
               const char* reason, const int wait_status, const bool timed_out,
               const char* output, bool* success)
{
    if (timed_out) {
        if (status == ATF_STATUS_EXPECTED_TIMEOUT) {
            *success = true;
            return kyua_result_write(
                output, KYUA_RESULT_EXPECTED_FAILURE, "%s", reason);
        } else {
            assert(status == ATF_STATUS_BROKEN);
            *success = false;
            return kyua_result_write(
                output, KYUA_RESULT_BROKEN, "Test case body timed out");
        }
    }

    switch (status) {
    case ATF_STATUS_BROKEN:
        return convert_broken(reason, wait_status, output, success);

    case ATF_STATUS_EXPECTED_DEATH:
        return convert_expected_death(reason, wait_status, output, success);

    case ATF_STATUS_EXPECTED_EXIT:
        return convert_expected_exit(status_arg, reason, wait_status, output,
                                     success);

    case ATF_STATUS_EXPECTED_FAILURE:
        return convert_expected_failure(reason, wait_status, output, success);

    case ATF_STATUS_EXPECTED_SIGNAL:
        return convert_expected_signal(status_arg, reason, wait_status, output,
                                       success);

    case ATF_STATUS_EXPECTED_TIMEOUT:
        return convert_expected_timeout(wait_status, output, success);

    case ATF_STATUS_FAILED:
        return convert_failed(reason, wait_status, output, success);

    case ATF_STATUS_PASSED:
        return convert_passed(wait_status, output, success);

    case ATF_STATUS_SKIPPED:
        return convert_skipped(reason, wait_status, output, success);
    }

    assert(false);
}


/// Writes a generic result file based on an ATF result file and an exit code.
///
/// \param input_name Path to the ATF result file to parse.
/// \param output_name Path to the generic result file to create.
/// \param wait_status Exit code of the test program as returned by wait().
/// \param timed_out Whether the test program timed out or not.
/// \param [out] success Whether the result should be considered a success or
///     not; e.g. passed and skipped are successful, but failed is not.
///
/// \return An error if the conversion fails; OK otherwise.
kyua_error_t
kyua_atf_result_rewrite(const char* input_name, const char* output_name,
                        const int wait_status, const bool timed_out,
                        bool* success)
{
    enum atf_status status; int status_arg; char reason[1024];
    status = ATF_STATUS_BROKEN;  // Initialize to shut up gcc warning.
    const kyua_error_t error = read_atf_result(input_name, &status, &status_arg,
                                               reason, sizeof(reason));
    if (kyua_error_is_set(error)) {
        // Errors while parsing the ATF result file can often be attributed to
        // the result file being bogus.  Therefore, just mark the test case as
        // broken, because it possibly is.
        status = ATF_STATUS_BROKEN;
        kyua_error_format(error, reason, sizeof(reason));
        kyua_error_free(error);
    }

    // Errors converting the loaded result to the final result file are not due
    // to a bad test program: they are because our own code fails (e.g. cannot
    // create the output file).  These need to be returned to the caller.
    return convert_result(status, status_arg, reason, wait_status, timed_out,
                          output_name, success);
}


/// Creates a result file for a failed cleanup routine.
///
/// This function is supposed to be invoked after the body has had a chance to
/// create its own result file, and only if the body has terminated with a
/// non-failure result.
///
/// \param output_name Path to the generic result file to create.
/// \param wait_status Exit code of the test program as returned by wait().
/// \param timed_out Whether the test program timed out or not.
/// \param [out] success Whether the result should be considered a success or
///     not; i.e. a clean exit is successful, but anything else is a failure.
///
/// \return An error if there is a problem writing the result; OK otherwise.
kyua_error_t
kyua_atf_result_cleanup_rewrite(const char* output_name, int wait_status,
                                const bool timed_out, bool* success)
{
    if (timed_out) {
        *success = false;
        return kyua_result_write(
            output_name, KYUA_RESULT_BROKEN, "Test case cleanup timed out");
    } else {
        if (WIFEXITED(wait_status)) {
            if (WEXITSTATUS(wait_status) == EXIT_SUCCESS) {
                *success = true;
                // Reuse the result file created by the body.  I.e. avoid
                // creating a new file here.
                return kyua_error_ok();
            } else {
                *success = false;
                return kyua_result_write(
                    output_name, KYUA_RESULT_BROKEN, "Test case cleanup exited "
                    "with code %d", WEXITSTATUS(wait_status));
            }
        } else {
            *success = false;
            return kyua_result_write(
                output_name, KYUA_RESULT_BROKEN, "Test case cleanup received "
                "signal %d%s", WTERMSIG(wait_status),
                WCOREDUMP(wait_status) ? " (core dumped)" : "");
        }
    }
}
