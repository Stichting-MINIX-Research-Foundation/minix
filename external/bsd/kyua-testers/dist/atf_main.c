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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atf_list.h"
#include "atf_result.h"
#include "cli.h"
#include "defs.h"
#include "error.h"
#include "fs.h"
#include "run.h"
#include "stacktrace.h"
#include "text.h"


/// Template for the creation of the temporary work directories.
#define WORKDIR_TEMPLATE "kyua.atf-tester.XXXXXX"


static void run_list(const char*, const int[2]) KYUA_DEFS_NORETURN;


/// Executes the test program in list mode.
///
/// \param test_program Path to the test program to execute; should be absolute.
/// \param stdout_fds Pipe to write the output of the test program to.
static void
run_list(const char* test_program, const int stdout_fds[2])
{
    (void)close(stdout_fds[0]);

    if (stdout_fds[1] != STDOUT_FILENO) {
        if (dup2(stdout_fds[1], STDOUT_FILENO) == -1)
            err(EXIT_FAILURE, "dup2 failed");
        (void)close(stdout_fds[1]);
    }

    if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
        err(EXIT_FAILURE, "dup2 failed");

    const char* const program_args[] = { test_program, "-l", NULL };
    kyua_run_exec(test_program, program_args);
}


/// Dumps the contents of the input file into the output.
///
/// \param input File from which to read.
/// \param output File to which to write.
///
/// \return An error if there is a problem.
static kyua_error_t
dump_file(FILE* input, FILE* output)
{
    char buffer[1024];
    size_t length;

    while ((length = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, length, output) != length) {
            return kyua_generic_error_new("Failed to write to output file");
        }
    }
    if (ferror(input))
        return kyua_libc_error_new(errno, "Failed to read test cases list");

    return kyua_error_ok();
}


/// Creates a file within the work directory.
///
/// \param work_directory Path to the work directory.
/// \param name Name of the file to create.
/// \param mode Mode of the file, as specified by fopen(3).
/// \param [out] file Pointer to the created stream.
///
/// \return An error if there is a problem.
static kyua_error_t
create_file_in_work_directory(const char* work_directory, const char* name,
                              const char* mode, FILE** file)
{
    char* path;
    kyua_error_t error = kyua_fs_concat(&path, work_directory, name, NULL);
    if (kyua_error_is_set(error))
        goto out;

    FILE* tmp_file = fopen(path, mode);
    if (tmp_file == NULL) {
        error = kyua_libc_error_new(errno, "Failed to create %s", path);
        goto out_path;
    }

    *file = tmp_file;

    assert(!kyua_error_is_set(error));
out_path:
    free(path);
out:
    return error;
}


/// Lists the test cases in a test program.
///
/// \param test_program Path to the test program for which to list the test
///     cases.  Should be absolute.
/// \param run_params Execution parameters to configure the test process.
///
/// \return An error if the listing fails; OK otherwise.
static kyua_error_t
list_test_cases(const char* test_program, const kyua_run_params_t* run_params)
{
    kyua_error_t error;

    char* work_directory;
    error = kyua_run_work_directory_enter(WORKDIR_TEMPLATE,
                                          run_params->unprivileged_user,
                                          run_params->unprivileged_group,
                                          &work_directory);
    if (kyua_error_is_set(error))
        goto out;
    kyua_run_params_t real_run_params = *run_params;
    real_run_params.work_directory = work_directory;

    int stdout_fds[2];
    if (pipe(stdout_fds) == -1) {
        error = kyua_libc_error_new(errno, "pipe failed");
        goto out_work_directory;
    }

    pid_t pid;
    error = kyua_run_fork(&real_run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        run_list(test_program, stdout_fds);
    }
    assert(pid != -1 && pid != 0);
    if (kyua_error_is_set(error))
        goto out_stdout_fds;

    FILE* tmp_output = NULL;  // Initialize to shut up gcc warning.
    error = create_file_in_work_directory(real_run_params.work_directory,
                                          "list.txt", "w+", &tmp_output);
    if (kyua_error_is_set(error))
        goto out_stdout_fds;

    close(stdout_fds[1]); stdout_fds[1] = -1;
    kyua_error_t parse_error = atf_list_parse(stdout_fds[0], tmp_output);
    stdout_fds[0] = -1;  // Guaranteed closed by atf_list_parse.
    // Delay reporting of parse errors to later.  If we detect a problem while
    // waiting for the test program, we know that the parsing has most likely
    // failed and therefore the error with the program is more important for
    // reporting purposes.

    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        goto out_tmp_output;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        error = kyua_generic_error_new("Test program list did not return "
                                       "success");
        goto out_tmp_output;
    }

    error = kyua_error_subsume(error, parse_error);
    if (!kyua_error_is_set(error)) {
        rewind(tmp_output);
        error = dump_file(tmp_output, stdout);
    }

out_tmp_output:
    fclose(tmp_output);
out_stdout_fds:
    if (stdout_fds[0] != -1)
        close(stdout_fds[0]);
    if (stdout_fds[1] != -1)
        close(stdout_fds[1]);
out_work_directory:
    error = kyua_error_subsume(error,
        kyua_run_work_directory_leave(&work_directory));
out:
    return error;
}


/// Counts the length of a user variables array.
///
/// \param user_variables The array of elements to be counted.
///
/// \return The length of the array.
static size_t
count_variables(const char* const user_variables[])
{
    size_t count = 0;
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter)
        count++;
    return count;
}


static void exec_body(const char* test_program, const char* test_case,
                      const char* result_file,
                      const char* const user_variables[]) KYUA_DEFS_NORETURN;


/// Executes the body of a test case.
///
/// \param test_program Path to the test program to execute.
/// \param test_case Name of the test case to run.
/// \param result_file Path to the ATF result file to be created.
/// \param user_variables Set of configuration variables to pass to the test.
static void
exec_body(const char* test_program, const char* test_case,
          const char* result_file, const char* const user_variables[])
{
    const size_t nargs =
        1 /* test_program */ +
        2 /* -r result_file */
        + 2 * count_variables(user_variables) /* -v name=value */
        + 1 /* test_case */ +
        1 /* NULL */;

    const char** args = malloc(sizeof(const char*) * nargs);
    if (args == NULL)
        kyua_error_err(EXIT_FAILURE, kyua_oom_error_new(),
                       "Failed to construct arguments list");

    size_t i = 0;
    args[i++] = test_program;
    args[i++] = "-r";
    args[i++] = result_file;
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        args[i++] = "-v";
        args[i++] = *iter;
    }
    args[i++] = test_case;
    args[i++] = NULL;
    assert(i == nargs);

    kyua_run_exec(test_program, args);
}


/// Forks and executes the body of a test case in a controlled manner.
///
/// \param test_program Path to the test program to execute.
/// \param test_case Name of the test case to run.
/// \param result_file Path to the ATF result file to be created.
/// \param user_variables Set of configuration variables to pass to the test.
/// \param run_params Settings to control the subprocess.
/// \param [out] success Set to true if the test case runs properly and returns
///     a result that is to be considered as successful.
///
/// \return OK if all goes well, an error otherwise.  Note that a failed test
/// case is denoted by setting success to false on exit, not by returning an
/// error.
static kyua_error_t
run_body(const char* test_program, const char* test_case,
         const char* result_file, const char* const user_variables[],
         const kyua_run_params_t* run_params, bool* success)
{
    kyua_error_t error;

    char* tmp_result_file;
    error = kyua_fs_concat(&tmp_result_file, run_params->work_directory,
                           "result.txt", NULL);
    if (kyua_error_is_set(error))
        goto out;

    pid_t pid;
    error = kyua_run_fork(run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        exec_body(test_program, test_case, tmp_result_file, user_variables);
    }
    assert(pid != -1 && pid != 0);
    if (kyua_error_is_set(error))
        goto out_tmp_result_file;

    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        goto out_tmp_result_file;

    if (WIFSIGNALED(status) && WCOREDUMP(status)) {
        kyua_stacktrace_dump(test_program, pid, run_params, stderr);
    }

    error = kyua_atf_result_rewrite(tmp_result_file, result_file, status,
                                    timed_out, success);

out_tmp_result_file:
    free(tmp_result_file);
out:
    return error;
}


static void exec_cleanup(const char* test_program, const char* test_case,
                         const char* const user_variables[]) KYUA_DEFS_NORETURN;


/// Executes the cleanup of a test case.
///
/// \param test_program Path to the test program to execute.
/// \param test_case Name of the test case to run.
/// \param user_variables Set of configuration variables to pass to the test.
static void
exec_cleanup(const char* test_program, const char* test_case,
             const char* const user_variables[])
{
    char* name;
    kyua_error_t error = kyua_text_printf(&name, "%s:cleanup", test_case);
    if (kyua_error_is_set(error))
        kyua_error_err(EXIT_FAILURE, error,
                       "Failed to construct argument list");

    const size_t nargs =
        1 /* test_program */ +
        + 2 * count_variables(user_variables) /* -v name=value */
        + 1 /* test_case */ +
        1 /* NULL */;

    const char** args = malloc(sizeof(const char*) * nargs);
    if (args == NULL)
        kyua_error_err(EXIT_FAILURE, kyua_oom_error_new(),
                       "Failed to construct arguments list");

    size_t i = 0;
    args[i++] = test_program;
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        args[i++] = "-v";
        args[i++] = *iter;
    }
    args[i++] = name;
    args[i++] = NULL;
    assert(i == nargs);

    kyua_run_exec(test_program, args);
}


/// Forks and executes the cleanup of a test case in a controlled manner.
///
/// \param test_program Path to the test program to execute.
/// \param test_case Name of the test case to run.
/// \param result_file Path to the ATF result file created by the body of the
///     test case.  The cleanup may update such file if it fails.
/// \param user_variables Set of configuration variables to pass to the test.
/// \param run_params Settings to control the subprocess.
/// \param body_success The success value returned by run_body().
/// \param [out] success Set to true if the test case runs properly and returns
///     a result that is to be considered as successful.
///
/// \return OK if all goes well, an error otherwise.  Note that a failed test
/// case cleanup is denoted by setting success to false on exit, not by
/// returning an error.
static kyua_error_t
run_cleanup(const char* test_program, const char* test_case,
            const char* result_file, const char* const user_variables[],
            const kyua_run_params_t* run_params, const bool body_success,
            bool* success)
{
    kyua_error_t error;

    pid_t pid;
    error = kyua_run_fork(run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        exec_cleanup(test_program, test_case, user_variables);
    }
    assert(pid != -1 && pid != 0);
    if (kyua_error_is_set(error))
        goto out;

    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        goto out;

    if (WIFSIGNALED(status) && WCOREDUMP(status)) {
        kyua_stacktrace_dump(test_program, pid, run_params, stderr);
    }

    if (body_success) {
        // If the body has reported a successful result, we inspect the status
        // of the cleanup routine.  If the cleanup has failed, then we need to
        // mark the test as broken.  However, if the body itself had failed, we
        // don't do this to give preference to the original result, which is
        // probably more informative.
        error = kyua_atf_result_cleanup_rewrite(result_file, status,
                                                timed_out, success);
    }

out:
    return error;
}


/// Checks if the user variables indicate that a test has a cleanup routine.
///
/// This is an ugly hack to allow us to run the cleanup routine only when a test
/// case has it.  When Kyua invokes the tester to generate the test case list,
/// the tester tells Kyua which tests have a cleanup routine.  However, when the
/// tests are later run from here (as a different invocation) we cannot know if
/// the test had a cleanup routine or not.  We rely on Kyua telling us this fact
/// by specifying has.cleanup=true in the variables.
///
/// \param user_variables Array of name=value pairs that describe the user
///     configuration variables for the test case.
static bool
has_cleanup(const char* const* user_variables)
{
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        if (strcmp(*iter, "has.cleanup=false") == 0)
            return false;
    }

    // The default is true because not running a cleanup routine when it exists
    // is worse than running an empty routine when not told to do so.
    return true;
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
              const kyua_run_params_t* run_params, bool* success)
{
    kyua_error_t error;

    char* work_directory;
    error = kyua_run_work_directory_enter(WORKDIR_TEMPLATE,
                                          run_params->unprivileged_user,
                                          run_params->unprivileged_group,
                                          &work_directory);
    if (kyua_error_is_set(error))
        goto out;
    kyua_run_params_t real_run_params = *run_params;
    real_run_params.work_directory = work_directory;

    error = run_body(test_program, test_case, result_file, user_variables,
                     &real_run_params, success);
    if (has_cleanup(user_variables)) {
        error = run_cleanup(test_program, test_case, result_file,
                            user_variables, &real_run_params, *success,
                            success);
    }

    error = kyua_error_subsume(error,
        kyua_run_work_directory_leave(&work_directory));
out:
    return error;
}


/// Definition of the tester.
static kyua_cli_tester_t atf_tester = {
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
    return kyua_cli_main(argc, argv, &atf_tester);
}
