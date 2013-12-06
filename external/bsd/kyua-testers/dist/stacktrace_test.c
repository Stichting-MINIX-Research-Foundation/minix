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

#include "stacktrace.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "env.h"
#include "error.h"
#include "fs.h"
#include "run.h"
#include "text.h"


/// Ensures that the given expression does not return a kyua_error_t.
///
/// \param expr Expression to evaluate.
#define RE(expr) ATF_REQUIRE(!kyua_error_is_set(expr))


/// Ensures that the given expression does not return a kyua_error_t.
///
/// \param expr Expression to evaluate.
/// \param msg Failure message.
#define RE_MSG(expr, msg) ATF_REQUIRE_MSG(!kyua_error_is_set(expr), msg)


/// Generates a core dump.
///
/// Due to the complexity of this interface, you should probably use
/// generate_core() instead.
///
/// \post If this fails to generate a core file, the test case is marked as
/// skipped.  The caller therefore can rely that a core dump has been created on
/// return.
///
/// \param tc Pointer to the caller test case.
/// \param run_params Parameters for the execution of the helper.
/// \param helper_path Path to the created helper.
/// \param exec_path Name of the helper, prefixed with ./ so that it can be
///     executed from within the work directory.
/// \param helper_name Basename of the helper.
///
/// \return The PID of the crashed binary.
static pid_t
generate_core_aux(const atf_tc_t* tc, const kyua_run_params_t* run_params,
                  const char* helper_path, const char* exec_path,
                  const char* helper_name)
{
    const char* srcdir = atf_tc_get_config_var(tc, "srcdir");

    char* src_helper;
    RE(kyua_fs_concat(&src_helper, srcdir, "stacktrace_helper", NULL));
    atf_utils_copy_file(src_helper, helper_path);
    free(src_helper);

    // We use kyua_run_fork for this to better simulate the final use case of
    // the stacktrace gathering, as test programs are run through these
    // functions.  Also, kyua_run_fork provides us with automatic unlimiting of
    // resources so that core files can be generated.

    pid_t pid;
    const kyua_error_t error = kyua_run_fork(run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        const char* const args[] = { helper_name, NULL };
        kyua_run_exec(exec_path, args);
    }
    RE(error);

    int status; bool timed_out;
    RE_MSG(kyua_run_wait(pid, &status, &timed_out),
           "wait failed; unexpected problem during exec?");

    ATF_REQUIRE(WIFSIGNALED(status));
    if (!WCOREDUMP(status))
        atf_tc_skip("Test failed to generate core dump");
    return pid;
}


/// Creates a script.
///
/// \param script Path to the script to create.
/// \param contents Contents of the script.
static void
create_script(const char* script, const char* contents)
{
    atf_utils_create_file(script, "#! /bin/sh\n\n%s\n", contents);
    ATF_REQUIRE(chmod(script, 0755) != -1);
}


/// Generates a core file.
///
/// \param tc Pointer to the calling test case.
/// \param work_directory Name of the directory in which to place the binary
///     that will generate the stacktrace.
/// \param program_name Basename of the binary that will crash.
///
/// \return PID of the process that generated the core file.
static pid_t
generate_core(const atf_tc_t* tc, const char* work_directory,
              const char* program_name)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    if (strcmp(work_directory, ".") != 0) {
        ATF_REQUIRE(mkdir(work_directory, 0755) != -1);
        run_params.work_directory = work_directory;
    }

    char* copy_to; char* exec_path;
    RE(kyua_text_printf(&copy_to, "%s/%s", work_directory, program_name));
    RE(kyua_text_printf(&exec_path, "./%s", program_name));
    const pid_t pid = generate_core_aux(tc, &run_params, copy_to, exec_path,
                                        program_name);
    free(exec_path);
    free(copy_to);
    return pid;
}


/// Prepares and runs kyua_stacktrace_dump().
///
/// \param tc Pointer to the calling test case.
/// \param work_directory Name of the directory in which to place the binary
///     that will generate the stacktrace.
/// \param program_name Basename of the binary that will crash.
/// \param output_name Name of the file to which to write the stacktrace.
/// \param timeout_seconds Time to give GDB to complete.
static void
do_dump(const atf_tc_t* tc, const char* work_directory,
        const char* program_name, const char* output_name,
        const int timeout_seconds)
{
    const pid_t pid = generate_core(tc, work_directory, program_name);

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.timeout_seconds = timeout_seconds + 100;  // Some large value.
    run_params.work_directory = work_directory;  // Created by generate_core.

    kyua_stacktrace_gdb_timeout = timeout_seconds;

    FILE* output = fopen(output_name, "w");
    ATF_REQUIRE(output != NULL);
    kyua_stacktrace_dump(program_name, pid, &run_params, output);
    fclose(output);
    atf_utils_cat_file(output_name, "dump output: ");
}


ATF_TC_WITHOUT_HEAD(find_core__found__short);
ATF_TC_BODY(find_core__found__short, tc)
{
    const pid_t pid = generate_core(tc, "dir", "short");
    const char* core_name = kyua_stacktrace_find_core("short", "dir", pid);
    if (core_name == NULL)
        atf_tc_fail("Core dumped, but no candidates found");
    ATF_REQUIRE(strstr(core_name, "core") != NULL);
    ATF_REQUIRE(access(core_name, F_OK) != -1);
}


ATF_TC_WITHOUT_HEAD(find_core__found__long);
ATF_TC_BODY(find_core__found__long, tc)
{
    const pid_t pid = generate_core(
        tc, "dir", "long-name-that-may-be-truncated-in-some-systems");
    const char* core_name = kyua_stacktrace_find_core(
        "long-name-that-may-be-truncated-in-some-systems", "dir", pid);
    if (core_name == NULL)
        atf_tc_fail("Core dumped, but no candidates found");
    ATF_REQUIRE(strstr(core_name, "core") != NULL);
    ATF_REQUIRE(access(core_name, F_OK) != -1);
}


ATF_TC_WITHOUT_HEAD(find_core__not_found);
ATF_TC_BODY(find_core__not_found, tc)
{
    const char* core_name = kyua_stacktrace_find_core("missing", ".", 1);
    if (core_name != NULL)
        atf_tc_fail("Core not dumped, but candidate found: %s", core_name);
}


ATF_TC_WITHOUT_HEAD(dump__integration);
ATF_TC_BODY(dump__integration, tc)
{
    do_dump(tc, "dir", "short", "stacktrace", 10);

    // It is hard to validate the execution of an arbitrary GDB of which we know
    // nothing anything.  Just assume that the backtrace, at the very least,
    // prints a frame identifier.
    ATF_REQUIRE(atf_utils_grep_file("#0", "stacktrace"));
}


ATF_TC_WITHOUT_HEAD(dump__ok);
ATF_TC_BODY(dump__ok, tc)
{
    RE(kyua_env_set("PATH", "."));
    create_script("fake-gdb", "echo 'frame 1'; echo 'frame 2'; "
                  "echo 'some warning' 1>&2; exit 0");
    kyua_stacktrace_gdb = "fake-gdb";

    do_dump(tc, ".", "short", "stacktrace", 10);

    ATF_REQUIRE(atf_utils_grep_file("dumped core; attempting to gather",
                                    "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("frame 1", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("frame 2", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("some warning", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("GDB exited successfully", "stacktrace"));
}


ATF_TC_WITHOUT_HEAD(dump__cannot_find_core);
ATF_TC_BODY(dump__cannot_find_core, tc)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);

    FILE* output = fopen("stacktrace", "w");
    ATF_REQUIRE(output != NULL);
    // This assumes that init(8) has never core dumped.
    kyua_stacktrace_dump("missing", 1, &run_params, output);
    fclose(output);
    atf_utils_cat_file("stacktrace", "dump output: ");

    ATF_REQUIRE(atf_utils_grep_file("Cannot find any core file", "stacktrace"));
}


ATF_TC_WITHOUT_HEAD(dump__cannot_find_gdb);
ATF_TC_BODY(dump__cannot_find_gdb, tc)
{
    RE(kyua_env_set("PATH", "."));
    kyua_stacktrace_gdb = "missing-gdb";

    do_dump(tc, ".", "dont-care", "stacktrace", 10);

    ATF_REQUIRE(atf_utils_grep_file("execvp failed", "stacktrace"));
}


ATF_TC_WITHOUT_HEAD(dump__gdb_fail);
ATF_TC_BODY(dump__gdb_fail, tc)
{
    RE(kyua_env_set("PATH", "."));
    create_script("fake-gdb", "echo 'foo'; echo 'bar' 1>&2; exit 56");
    kyua_stacktrace_gdb = "fake-gdb";

    do_dump(tc, ".", "short", "stacktrace", 10);

    ATF_REQUIRE(atf_utils_grep_file("foo", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("bar", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("GDB failed with code 56; see output above "
                                    "for details", "stacktrace"));
}


ATF_TC_WITHOUT_HEAD(dump__gdb_times_out);
ATF_TC_BODY(dump__gdb_times_out, tc)
{
    RE(kyua_env_set("PATH", "."));
    create_script("fake-gdb", "echo 'foo'; echo 'bar' 1>&2; "
                  "/bin/sleep 10; /usr/bin/sleep 10; exit 0");
    kyua_stacktrace_gdb = "fake-gdb";

    do_dump(tc, ".", "short", "stacktrace", 1);

    ATF_REQUIRE(atf_utils_grep_file("foo", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("bar", "stacktrace"));
    ATF_REQUIRE(atf_utils_grep_file("GDB failed; timed out", "stacktrace"));
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, find_core__found__short);
    ATF_TP_ADD_TC(tp, find_core__found__long);
    ATF_TP_ADD_TC(tp, find_core__not_found);

    ATF_TP_ADD_TC(tp, dump__integration);
    ATF_TP_ADD_TC(tp, dump__ok);
    ATF_TP_ADD_TC(tp, dump__cannot_find_core);
    ATF_TP_ADD_TC(tp, dump__cannot_find_gdb);
    ATF_TP_ADD_TC(tp, dump__gdb_fail);
    ATF_TP_ADD_TC(tp, dump__gdb_times_out);

    return atf_no_error();
}
