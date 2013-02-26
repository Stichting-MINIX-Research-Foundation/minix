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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include "run.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "env.h"
#include "error.h"
#include "fs.h"
#include "text.h"


/// Path to the temporary work directory to use.
const char* kyua_run_tmpdir = KYUA_TMPDIR;
#undef KYUA_TMPDIR  // We really want to use the variable, not the macro.


/// Whether we got a signal or not.
static volatile bool signal_fired = false;


/// Whether the process timed out or not.
static volatile bool process_timed_out = false;


/// If not -1, PID of the process to forcibly kill when we get a signal.
///
/// Must be protected by protect() and unprotect().
static volatile pid_t pid_to_kill = -1;


/// Whether we are holding signals or not.
static bool protected = false;


/// Magic exit code to denote an error while preparing the subprocess.
static const int exit_setup_child = 124;
/// Magic exit code to denote an error in exec(3) that we do not handle.
static const int exit_exec_unknown = 123;
/// Magic exit code to denote an EACCES error in exec(3).
static const int exit_exec_eacces = 122;
/// Magic exit code to denote an ENOENT error in exec(3).
static const int exit_exec_enoent = 121;


/// Area to save the original SIGHUP handler.
static struct sigaction old_sighup;
/// Area to save the original SIGINT handler.
static struct sigaction old_sigint;
/// Area to save the original SIGTERM handler.
static struct sigaction old_sigterm;
/// Area to save the original SIGALRM handler.
static struct sigaction old_sigalrm;
/// Area to save the original realtime timer.
static struct itimerval old_timer;


/// Masks or unmasks all the signals programmed by this module.
///
/// \param operation One of SIG_BLOCK or SIG_UNBLOCK.
static void
mask_handlers(const int operation)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    const int ret = sigprocmask(operation, &mask, NULL);
    assert(ret != -1);
}


/// Masks all signals programmed by this module.
static void
protect(void)
{
    mask_handlers(SIG_BLOCK);
    protected = true;
}


/// Unmasks all signals programmed by this module.
static void
unprotect(void)
{
    protected = false;
    mask_handlers(SIG_UNBLOCK);
}


/// Handler for signals that should abort execution.
///
/// When called the first time, this handler kills any running subprocess so
/// that the cleanup routines can proceed.  Calling this a second time aborts
/// execution of the program.
///
/// \param unused_signo Number of the captured signal.
static void
cleanup_handler(const int KYUA_DEFS_UNUSED_PARAM(signo))
{
    static const char* clean_message = "Signal caught; cleaning up...\n";
    static const char* abort_message = "Double signal caught; aborting...\n";

    protect();
    if (!signal_fired) {
        signal_fired = true;
        if (write(STDERR_FILENO, clean_message, strlen(clean_message)) == -1) {
            // Ignore.
        }
        if (pid_to_kill != -1) {
            kill(pid_to_kill, SIGKILL);
            pid_to_kill = -1;
        }
        unprotect();
    } else {
        if (write(STDERR_FILENO, abort_message, strlen(abort_message)) == -1) {
            // Ignore.
        }
        if (pid_to_kill != -1) {
            kill(pid_to_kill, SIGKILL);
            pid_to_kill = -1;
        }
        abort();
    }
}


/// Handler for signals that should terminate the active subprocess.
///
/// \param unused_signo Number of the captured signal.
static void
timeout_handler(const int KYUA_DEFS_UNUSED_PARAM(signo))
{
    static const char* message = "Subprocess timed out; sending KILL "
        "signal...\n";

    protect();
    process_timed_out = true;
    if (write(STDERR_FILENO, message, strlen(message)) == -1) {
        // Ignore.
    }
    if (pid_to_kill != -1) {
        kill(pid_to_kill, SIGKILL);
        pid_to_kill = -1;
    }
    unprotect();
}


/// Installs a signal handler.
///
/// \param signo Number of the signal to program.
/// \param handler Handler for the signal.
/// \param [out] old_sa Pointer to the sigaction structure in which to save the
///     current signal handler data.
static void
setup_signal(const int signo, void (*handler)(const int),
             struct sigaction* old_sa)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    const int ret = sigaction(signo, &sa, old_sa);
    assert(ret != -1);
}


/// Installs a timer.
///
/// \param seconds Deadline for the timer.
/// \param [out] old_itimerval Pointer to the itimerval structure in which to
///     save the current timer data into.
static void
setup_timer(const int seconds, struct itimerval* old_itimerval)
{
    struct itimerval new_timer;
    new_timer.it_interval.tv_sec = 0;
    new_timer.it_interval.tv_usec = 0;
    new_timer.it_value.tv_sec = seconds;
    new_timer.it_value.tv_usec = 0;
    const int ret = setitimer(ITIMER_REAL, &new_timer, old_itimerval);
    assert(ret != -1);
}


/// Resets the environment of the process to a known state.
///
/// \param work_directory Path to the work directory being used.
///
/// \return An error if there is a problem configuring the environment
/// variables, or OK if successful.  Note that if this returns an error, we have
/// left the environment in an inconsistent state.
static kyua_error_t
prepare_environment(const char* work_directory)
{
    kyua_error_t error;

    // TODO(jmmv): It might be better to do the opposite: just pass a good known
    // set of variables to the child (aka HOME, PATH, ...).  But how do we
    // determine this minimum set?

    const char* to_unset[] = { "LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
                               "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC",
                               "LC_TIME", NULL };
    const char** iter;
    for (iter = to_unset; *iter != NULL; ++iter) {
        error = kyua_env_unset(*iter);
        if (kyua_error_is_set(error))
            return error;
    }

    error = kyua_env_set("HOME", work_directory);
    if (kyua_error_is_set(error))
        return error;

    error = kyua_env_set("TZ", "UTC");
    if (kyua_error_is_set(error))
        return error;

    error = kyua_env_set("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");
    if (kyua_error_is_set(error))
        return error;

    assert(!kyua_error_is_set(error));
    return error;
}


/// Resets all signals to their default handlers.
static void
reset_signals(void)
{
    int signo;

    for (signo = 1; signo <= LAST_SIGNO; ++signo) {
        if (signo == SIGKILL || signo == SIGSTOP) {
            // Don't attempt to reset immutable signals.
            continue;
        }

        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        (void)sigaction(signo, &sa, NULL);
    }
}


/// Raises core size limit to its possible maximum.
///
/// This is a best-effort operation.  There is no guarantee that the operation
/// will yield a large-enough limit to generate any possible core file.
static void
unlimit_core_size(void)
{
#if !defined(__minix)
    struct rlimit rl;
    {
        const int ret = getrlimit(RLIMIT_CORE, &rl);
        assert(ret != -1);
    }
    rl.rlim_cur = rl.rlim_max;
    const int ret = setrlimit(RLIMIT_CORE, &rl) != -1;
    assert(ret != -1);
#endif /* !defined(__minix) */
}


/// Cleans up the container process to run a new child.
///
/// If there is any error during the setup, the new process is terminated
/// with an error code.
///
/// \param run_params End-user parameters that describe how to isolate the
///     newly-created process.
static void
setup_child(const kyua_run_params_t* run_params)
{
#if !defined(__minix)
    setpgid(getpid(), getpid());
#endif /* !defined(__minix) */

    if (chdir(run_params->work_directory) == -1)
        err(exit_setup_child, "chdir(%s) failed", run_params->work_directory);

    unlimit_core_size();
    reset_signals();

    const kyua_error_t error = prepare_environment(run_params->work_directory);
    if (kyua_error_is_set(error))
        kyua_error_err(exit_setup_child, error, "Failed to configure the "
                       "environment");

    (void)umask(0022);

    if (run_params->unprivileged_group != getgid()) {
        if (setgid(run_params->unprivileged_group) == -1)
            err(exit_setup_child, "setgid(%ld) failed; uid is %ld and gid "
                "is %ld", (long int)run_params->unprivileged_group,
                (long int)getuid(), (long int)getgid());
    }
    if (run_params->unprivileged_user != getuid()) {
        if (setuid(run_params->unprivileged_user) == -1)
            err(exit_setup_child, "setuid(%ld) failed; uid is %ld and gid "
                "is %ld", (long int)run_params->unprivileged_user,
                (long int)getuid(), (long int)getgid());
    }
}


/// Constructs a path to a work directory based on a template.
///
/// \param template Template of the work directory to create.  Should be a
///      basename and must include the XXXXXX placeholder string.  The directory
///      is created within the temporary directory specified by the TMPDIR
///      environment variable or the builtin value in kyua_run_tmpdir.
/// \param [out] work_directory Pointer to a dynamically-allocated string
///      containing the full path to the work directory.  The caller is
///      responsible for releasing this string.
///
/// \return An error if there is a problem (most likely OOM), or OK otherwise.
static kyua_error_t
build_work_directory_path(const char* template, char** work_directory)
{
    assert(strstr(template, "XXXXXX") != NULL);

    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL)
        return kyua_text_printf(work_directory, "%s/%s", kyua_run_tmpdir,
                                template);
    else
        return kyua_text_printf(work_directory, "%s/%s", tmpdir, template);
}


/// Initializes the run_params parameters with default values.
///
/// \param [out] run_params The object to initialize.
void
kyua_run_params_init(kyua_run_params_t* run_params)
{
    run_params->timeout_seconds = 60;
    run_params->unprivileged_user = getuid();
    run_params->unprivileged_group = getgid();
    run_params->work_directory = ".";
}


/// Executes a program and exits if there is a problem.
///
/// This routine is supposed to be used in conjunction with kyua_run_fork and
/// kyua_run_wait so that the various return codes of the exec system call are
/// properly propagated to the parent process.
///
/// \param program Path to the program to run.
/// \param args Arguments to the program.
void
kyua_run_exec(const char* program, const char* const* args)
{
    (void)execvp(program, KYUA_DEFS_UNCONST(args));
    switch (errno) {
    case EACCES:
        exit(exit_exec_eacces);
    case ENOENT:
        exit(exit_exec_enoent);
    default:
        err(exit_exec_unknown, "execvp failed");
    }
}


/// Forks and isolates the child process.
///
/// The created subprocess must be waited for with kyua_run_wait().
///
/// \param run_params Parameters that describe how to isolate the newly-created
///     process.
/// \param [out] out_pid The PID of the child in the parent, or 0 in the child.
///     The value left here should only be accessed if this function does not
///     return an error.
///
/// \return An error object if fork(2) fails.
kyua_error_t
kyua_run_fork(const kyua_run_params_t* run_params, pid_t* const out_pid)
{
    protect();
    pid_t pid = fork();
    if (pid == -1) {
        unprotect();
        *out_pid = pid;  // Not necessary, but avoid mistakes in the caller.
        return kyua_libc_error_new(errno, "fork failed");
    } else if (pid == 0) {
        unprotect();
        setup_child(run_params);
        *out_pid = pid;
        return kyua_error_ok();
    } else {
        pid_to_kill = pid;
        unprotect();

        setup_signal(SIGALRM, timeout_handler, &old_sigalrm);
        process_timed_out = false;
        setup_timer(run_params->timeout_seconds, &old_timer);

        *out_pid = pid;
        return kyua_error_ok();
    }
}


/// Waits for a process started via kyua_run_fork.
///
/// \param pid The PID of the child to wait for.
/// \param [out] status The exit status of the awaited process.
/// \param [out] timed_out Whether the process timed out or not.
///
/// \return An error if the process failed due to an problem in kyua_run_exec.
/// However, note that the wait for the process itself is expected to have been
/// successful.
kyua_error_t
kyua_run_wait(const pid_t pid, int* status, bool* timed_out)
{
    int tmp_status;
    const pid_t waited_pid = waitpid(pid, &tmp_status, 0);
    assert(pid == waited_pid);

    protect();
    (void)setitimer(ITIMER_REAL, &old_timer, NULL);
    (void)sigaction(SIGALRM, &old_sigalrm, NULL);
    pid_to_kill = -1;
    unprotect();

    killpg(pid, SIGKILL);

    if (WIFEXITED(tmp_status)) {
        if (WEXITSTATUS(tmp_status) == exit_setup_child) {
            return kyua_generic_error_new("Failed to isolate subprocess; "
                                          "see stderr for details");
        } else if (WEXITSTATUS(tmp_status) == exit_exec_eacces) {
            return kyua_libc_error_new(EACCES, "execvp failed");
        } else if (WEXITSTATUS(tmp_status) == exit_exec_enoent) {
            return kyua_libc_error_new(ENOENT, "execvp failed");
        } else if (WEXITSTATUS(tmp_status) == exit_exec_unknown) {
            return kyua_generic_error_new("execvp failed; see stderr for "
                                          "details");
        } else {
            // Fall-through.
        }
    }
    *status = tmp_status;
    *timed_out = process_timed_out;
    return kyua_error_ok();
}


/// Creates a temporary directory for use by a subprocess.
///
/// The temporary directory must be deleted with kyua_run_work_directory_leave.
///
/// \param template Template of the work directory to create.  Should be a
///      basename and must include the XXXXXX placeholder string.  The directory
///      is created within the temporary directory specified by the TMPDIR
///      environment variable or the builtin value.
/// \param uid User to set the owner of the directory to.
/// \param gid Group to set the owner of the directory to.
/// \param [out] out_work_directory Updated with a pointer to a dynamic string
///      holding the path to the created work directory.  This must be passed as
///      is to kyua_run_work_directory_leave, which takes care of freeing the
///      memory.
///
/// \return An error code if there is a problem creating the directory.
kyua_error_t
kyua_run_work_directory_enter(const char* template, const uid_t uid,
                              const gid_t gid, char** out_work_directory)
{
    kyua_error_t error = kyua_error_ok();

    signal_fired = false;
    setup_signal(SIGHUP, cleanup_handler, &old_sighup);
    setup_signal(SIGINT, cleanup_handler, &old_sigint);
    setup_signal(SIGTERM, cleanup_handler, &old_sigterm);

    char* work_directory;
    error = build_work_directory_path(template, &work_directory);
    if (kyua_error_is_set(error))
        goto err_signals;

    if (mkdtemp(work_directory) == NULL) {
        error = kyua_libc_error_new(errno, "mkdtemp(%s) failed",
                                    work_directory);
        goto err_work_directory_variable;
    }

    if (uid != getuid() || gid != getgid()) {
        if (chown(work_directory, uid, gid) == -1) {
            error = kyua_libc_error_new(errno,
                "chown(%s, %ld, %ld) failed; uid is %ld and gid is %ld",
                work_directory, (long int)uid, (long int)gid,
                (long int)getuid(), (long int)getgid());
            goto err_work_directory_file;
        }
    }

    *out_work_directory = work_directory;
    assert(!kyua_error_is_set(error));
    goto out;

err_work_directory_file:
    (void)rmdir(work_directory);
err_work_directory_variable:
    free(work_directory);
err_signals:
    (void)sigaction(SIGTERM, &old_sigterm, NULL);
    (void)sigaction(SIGINT, &old_sigint, NULL);
    (void)sigaction(SIGHUP, &old_sighup, NULL);
out:
    return error;
}


/// Deletes a temporary directory created by kyua_run_work_directory_enter().
///
/// \param [in,out] work_directory The pointer to the work_directory string as
///     originally returned by kyua_run_work_directory_leave().  This is
///     explicitly invalidated by this function to clearly denote that this
///     performs the memory releasing.
///
/// \return An error code if the cleanup of the directory fails.  Note that any
/// intermediate errors during the cleanup are sent to stderr.
kyua_error_t
kyua_run_work_directory_leave(char** work_directory)
{
    kyua_error_t error = kyua_fs_cleanup(*work_directory);

    free(*work_directory);
    *work_directory = NULL;

    (void)sigaction(SIGTERM, &old_sigterm, NULL);
    (void)sigaction(SIGHUP, &old_sighup, NULL);
    (void)sigaction(SIGINT, &old_sigint, NULL);

    // At this point, we have cleaned up the work directory and we could
    // (should?)  re-deliver the signal to ourselves so that we terminated with
    // the right code.  However, we just let this return and allow the caller
    // code to perform any other cleanups instead.

    return error;
}
