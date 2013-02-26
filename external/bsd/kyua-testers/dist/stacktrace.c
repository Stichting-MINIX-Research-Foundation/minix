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

#include <sys/param.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "fs.h"
#include "env.h"
#include "error.h"
#include "run.h"
#include "text.h"


/// Built-in path to GDB.
///
/// This should be an absolute path for deterministic behavior.  We also accept
/// a basename to cope with any issues that might arise from an invalid
/// configure check or a manual override of the GDB constant, in which case the
/// exec call below will (try to) locate the binary in the path.
///
/// Note that the program pointed to by this variable is not required to exist.
/// If it does not, we fail gracefully.
///
/// Test cases can override the value of this built-in constant to unit-test the
/// behavior of the functions below.
const char* kyua_stacktrace_gdb = GDB;
#undef GDB  // We really want to use the variable, not the macro.


/// Time to give to the external GDB process to produce a stack trace.
///
/// Test cases can override the value of this built-in constant to unit-test the
/// behavior of the functions below.
unsigned long kyua_stacktrace_gdb_timeout = 300;


/// Maximum length of the core file name, if known.
///
/// Some operating systems impose a maximum length on the basename of the core
/// file.  If MAXCOMLEN is defined, then we need to truncate the program name to
/// this length before searching for the core file.  If we cannot figure out
/// what this limit is, we set it to zero, which we consider later as
/// "unlimited".
#if !defined(MAXCOMLEN)
#   define MAXCOMLEN 0
#endif


static void run_gdb(const char* program, const char* core_name, FILE* output)
    KYUA_DEFS_NORETURN;


/// Constructs the parameters to run GDB with.
///
/// \param original_run_params Parameters used to run the binary that generated
///     the core dump.
///
/// \return The run parameters with which to run GDB.
static kyua_run_params_t
gdb_run_params(const kyua_run_params_t* original_run_params)
{
    kyua_run_params_t run_params = *original_run_params;
    run_params.timeout_seconds = kyua_stacktrace_gdb_timeout;
    return run_params;
}


/// Body of a subprocess to execute GDB.
///
/// This should be called from the child created by a kyua_run_fork() call,
/// which means that we do not have to take care of isolating the process.
///
/// \pre The caller must have flushed stdout before spawning this process, to
///     prevent double-flushing and/or corruption of data.
///
/// \param program Path to the program being debugged.  Can be relative to
///     the given work directory.
/// \param core_name Path to the dumped core.  Use find_core() to deduce
///     a valid candidate.  Can be relative to the given work directory.
/// \param output Stream to which to send the output of GDB.
static void
run_gdb(const char* program, const char* core_name, FILE* output)
{
    // TODO(jmmv): Should be done by kyua_run_fork(), but doing so would change
    // the semantics of the ATF interface.  Need to evaluate this carefully.
    const kyua_error_t error = kyua_env_unset("TERM");
    if (kyua_error_is_set(error)) {
        kyua_error_warn(error, "Failed to unset TERM; GDB may misbehave");
        free(error);
    }

    (void)close(STDIN_FILENO);
    const int input_fd = open("/dev/null", O_RDONLY);
    assert(input_fd == STDIN_FILENO);

    const int output_fd = fileno(output);
    assert(output_fd != -1);  // We expect a file-backed stream.
    if (output_fd != STDOUT_FILENO) {
        fflush(stdout);
        (void)dup2(output_fd, STDOUT_FILENO);
    }
    if (output_fd != STDERR_FILENO) {
        fflush(stderr);
        (void)dup2(output_fd, STDERR_FILENO);
    }
    if (output_fd != STDOUT_FILENO && output_fd != STDERR_FILENO)
        fclose(output);

    const char* const gdb_args[] = {
        "gdb", "-batch", "-q", "-ex", "bt", program, core_name, NULL };
    kyua_run_exec(kyua_stacktrace_gdb, gdb_args);
}


/// Truncates a string.
///
/// \param source The string to truncate.
/// \param [out] buffer Output buffer into which to store the truncated text.
/// \param buffer_length Size of the buffer.
///
/// \return A pointer to the buffer.
static const char*
slice(const char* source, char* buffer, const size_t buffer_length)
{
    const size_t source_length = strlen(source);
    if (source_length < buffer_length) {
        strcpy(buffer, source);
    } else {
        memcpy(buffer, source, buffer_length - 1);
        buffer[buffer_length - 1] = '\0';
    }
    return buffer;
}


static char* try_core(const char* format, ...) KYUA_DEFS_FORMAT_PRINTF(1, 2);


/// Generates a path and checks if it exists.
///
/// \param format Formatting string for the path to generate.
/// \param ... Arguments to the formatting string.
///
/// \return A dynamically-allocated string containing the generated path if
/// there were no errors and the file pointed to by such path exists; NULL
/// otherwise.  The returned string must be relesed with free() by the caller.
static char*
try_core(const char* format, ...)
{
    char* path;
    va_list ap;

    va_start(ap, format);
    kyua_error_t error = kyua_text_vprintf(&path, format, ap);
    va_end(ap);
    if (kyua_error_is_set(error)) {
        // Something went really wrong (and should not have happened).  Ignore
        // this core file candidate.
        kyua_error_free(error);
        return NULL;
    }

    if (access(path, F_OK) == -1) {
        free(path);
        return NULL;
    } else {
        return path;
    }
}


/// Simple version of basename() that operates on constant strings.
///
/// This is not 100% compatible with basename() because it may return an
/// unexpected string if the path ends with a slash.  For our purposes, this
/// does not matter, so we can use this simplified trick.
///
/// \param path Path from which to compute the basename.
///
/// \return A pointer within the input path pointing at the last component.
static const char*
const_basename(const char* path)
{
    const char* last_slash = strrchr(path, '/');
    return last_slash == NULL ? path : last_slash + 1;
}


/// Looks for a core file for the given program.
///
/// \param name The basename of the binary that generated the core.
/// \param directory The directory from which the program was run.  We expect to
///     find the core file in this directory.
/// \param dead_pid PID of the process that generated the core.  This is needed
///     in some platforms.
///
/// \return The path to the core file if found; otherwise none.
char*
kyua_stacktrace_find_core(const char* name, const char* directory,
                          const pid_t dead_pid)
{
    char* candidate = NULL;

    // TODO(jmmv): Other than checking all these defaults, in NetBSD we should
    // also inspect the value of the kern.defcorename sysctl(2) MIB and use that
    // as the first candidate.
    //
    // In Linux, the way to determine the name is by looking at
    // /proc/sys/kernel/core_{pattern,uses_pid} as described by core(5).
    // Unfortunately, there does not seem to be a standard API to parse these
    // files, which makes checking for core files quite difficult if the
    // defaults have been modified.

    // Default NetBSD naming scheme.
    if (candidate == NULL && MAXCOMLEN > 0) {
        char truncated[MAXCOMLEN + 1];
        candidate = try_core("%s/%s.core", directory,
                             slice(name, truncated, sizeof(truncated)));
    }

    // Common naming scheme without the MAXCOMLEN truncation.
    if (candidate == NULL)
        candidate = try_core("%s/%s.core", directory, name);

    // Common naming scheme found in Linux systems.
    if (candidate == NULL)
        candidate = try_core("%s/core.%d", directory, (int)dead_pid);

    // Default Mac OS X naming scheme.
    if (candidate == NULL)
        candidate = try_core("/cores/core.%d", (int)dead_pid);

    // Common naming scheme found in Linux systems.  Attempted last due to the
    // genericity of the core file name.
    if (candidate == NULL)
        candidate = try_core("%s/core", directory);

    if (candidate != NULL) {
        char* abs_candidate;
        kyua_error_t error = kyua_fs_make_absolute(candidate, &abs_candidate);
        if (kyua_error_is_set(error)) {
            kyua_error_free(error);
            return candidate;  // Return possibly-relative path as a best guess.
        } else {
            free(candidate);
            return abs_candidate;
        }
    } else {
        return candidate;
    }
}


/// Gathers a stacktrace of a crashed program.
///
/// \param program The name of the binary that crashed and dumped a core file.
///     Can be either absolute or relative.
/// \param dead_pid The PID of the process that dumped core.
/// \param original_run_params Parameters with which the original binary was
///     executed.  These are reused to run GDB, but adjusted with GDB-specific
///     settings.  Of special interest, the work directory is used to search for
///     the core file.
/// \param output Stream into which to dump the stack trace and any additional
///     information.
///
/// \post If anything goes wrong, the diagnostic messages are written to the
/// output.  This function returns no errors.
void
kyua_stacktrace_dump(const char* program, const pid_t dead_pid,
                     const kyua_run_params_t* original_run_params, FILE* output)
{
    fprintf(output, "Process with PID %d dumped core; attempting to gather "
            "stack trace\n", dead_pid);

    const kyua_run_params_t run_params = gdb_run_params(original_run_params);

    kyua_error_t error = kyua_error_ok();

    char* core_file = kyua_stacktrace_find_core(const_basename(program),
                                                run_params.work_directory,
                                                dead_pid);
    if (core_file == NULL) {
        fprintf(output, "Cannot find any core file\n");
        goto out;
    }

    // We must flush the output stream right before invoking fork, so that the
    // subprocess does not have any unflushed data.  Failure to do so results in
    // the messages above being written twice to the output.
    fflush(output);
    pid_t pid;
    error = kyua_run_fork(&run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        run_gdb(program, core_file, output);
    }
    assert(pid != -1 && pid != 0);
    if (kyua_error_is_set(error))
        goto out_core_file;

    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        goto out_core_file;

    if (timed_out) {
        fprintf(output, "GDB failed; timed out\n");
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EXIT_SUCCESS)
                fprintf(output, "GDB exited successfully\n");
            else
                fprintf(output, "GDB failed with code %d; see output above for "
                        "details\n", WEXITSTATUS(status));
        } else {
            assert(WIFSIGNALED(status));
            fprintf(output, "GDB received signal %d; see output above for "
                    "details\n", WTERMSIG(status));
        }
    }

out_core_file:
    free(core_file);
out:
    if (kyua_error_is_set(error)) {
        kyua_error_fprintf(output, error, "Failed to gather stacktrace");
        free(error);
    }
}
