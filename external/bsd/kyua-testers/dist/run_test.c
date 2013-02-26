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
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#include "defs.h"
#include "env.h"
#include "error.h"
#include "fs.h"

#if !defined(WCOREDUMP) && defined(__minix)
#define WCOREDUMP(x) ((x) & 0x80)
#endif /*!defined(WCOREDUMP) && defined(__minix) */


/// Evalutes an expression and ensures it does not return an error.
///
/// \param expr A expression that must evaluate to kyua_error_t.
#define RE(expr) ATF_REQUIRE(!kyua_error_is_set(expr))


static void check_env(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates the cleanliness of the environment.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the environment is clean; failure otherwise.
static void
check_env(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    bool failed = false;

    const char* empty[] = { "LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
                            "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC",
                            "LC_TIME", NULL };
    const char** iter;
    for (iter = empty; *iter != NULL; ++iter) {
        if (getenv(*iter) != NULL) {
            failed = true;
            printf("%s was not unset\n", *iter);
        }
    }

    if (strcmp(getenv("HOME"), ".") != 0) {
        failed = true;
        printf("HOME was not set to .\n");
    }
    if (strcmp(getenv("TZ"), "UTC") != 0) {
        failed = true;
        printf("TZ was not set to UTC\n");
    }
    if (strcmp(getenv("LEAVE_ME_ALONE"), "kill-some-day") != 0) {
        failed = true;
        printf("LEAVE_ME_ALONE was modified while it should not have been\n");
    }

    exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}


static void check_process_group(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that it has become the leader of a process group.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the process lives in its own process group;
/// failure otherwise.
static void
check_process_group(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
#if defined(__minix)
    // no pgid support on MINIX
    exit(EXIT_FAILURE);
#else
    exit(getpgid(getpid()) == getpid() ? EXIT_SUCCESS : EXIT_FAILURE);
#endif /* defined(__minix) */
}


static void check_signals(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that signals have been reset to their defaults.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the process has its signals reset to their
/// default values; failure otherwise.
static void
check_signals(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    int signo;
    for (signo = 1; signo <= LAST_SIGNO; signo++) {
        if (signo == SIGKILL || signo == SIGSTOP) {
            // Don't attempt to check immutable signals, as this results in
            // an unconditional error in some systems.  E.g. Mac OS X 10.8
            // reports 'Invalid argument' when querying SIGKILL.
            continue;
        }

        struct sigaction old_sa;
        if (sigaction(signo, NULL, &old_sa) == -1) {
            err(EXIT_FAILURE, "Failed to query signal information for %d",
                signo);
        }
        if (old_sa.sa_handler != SIG_DFL) {
            errx(EXIT_FAILURE, "Signal %d not reset to its default handler",
                 signo);
        }
        printf("Signal %d has its default handler set\n", signo);
    }

    exit(EXIT_SUCCESS);
}


static void check_umask(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that the umask has been reset.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the umask matches the expected value; failure
/// otherwise.
static void
check_umask(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    exit(umask(0) == 0022 ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void check_work_directory(const void* cookie) KYUA_DEFS_NORETURN;


/// Subprocess that validates that the umask has been reset.
///
/// \param cookie The name of a file to expect in the current directory.
///
/// \post Exits with success if the umask matches the expected value; failure
/// otherwise.
static void
check_work_directory(const void* cookie)
{
    const char* exp_file = (const char*)cookie;
    exit(atf_utils_file_exists(exp_file) ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void check_uid_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that the UID is not root.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the UID is not root; failure otherwise.
static void
check_uid_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    exit(getuid() != 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void check_gid_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that the GID is not root.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the GID is not root; failure otherwise.
static void
check_gid_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    exit(getgid() != 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void check_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
    KYUA_DEFS_NORETURN;


/// Subprocess that validates that the UID and GID are not root.
///
/// \param unused_cookie NULL.
///
/// \post Exits with success if the UID and GID are not root; failure otherwise.
static void
check_not_root(const void* KYUA_DEFS_UNUSED_PARAM(cookie))
{
    exit(getuid() != 0 && getgid() != 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


/// Uses kyua_fork, kyua_exec and kyua_wait to execute a subprocess.
///
/// \param program Path to the program to run.
/// \param args Arguments to the program.
/// \param [out] exitstatus The exit status of the subprocess, if it exits
///     successfully without timing out nor receiving a signal.
///
/// \return Returns the error code of kyua_run_wait (which should have the
///     error representation of the exec call in the subprocess).
static kyua_error_t
exec_check(const char* program, const char* const* args, int* exitstatus)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);

    pid_t pid;
    kyua_error_t error = kyua_run_fork(&run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0)
        kyua_run_exec(program, args);
    ATF_REQUIRE(!kyua_error_is_set(error));
    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (!kyua_error_is_set(error)) {
        ATF_REQUIRE(!timed_out);
        ATF_REQUIRE_MSG(WIFEXITED(status),
                        "Subprocess expected to exit successfully");
        *exitstatus = WEXITSTATUS(status);
    }
    return error;
}


/// Uses kyua_fork and kyua_wait to spawn a subprocess.
///
/// \param run_params The parameters to configure the subprocess.  Can be NULL
///     to indicate to use the default set of parameters.
/// \param hook Any of the check_* functions provided in this module.
/// \param cookie The data to pass to the hook.
///
/// \return True if the subprocess exits successfully; false otherwise.
static bool
fork_check(const kyua_run_params_t* run_params,
           void (*hook)(const void*), const void* cookie)
{
    kyua_run_params_t default_run_params;
    if (run_params == NULL) {
        kyua_run_params_init(&default_run_params);
        run_params = &default_run_params;
    }

    pid_t pid;
    kyua_error_t error = kyua_run_fork(run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0)
        hook(cookie);
    ATF_REQUIRE(!kyua_error_is_set(error));
    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        atf_tc_fail("wait failed; unexpected problem during exec?");
    ATF_REQUIRE(!timed_out);
    return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
}


ATF_TC_WITHOUT_HEAD(run_params_init__defaults);
ATF_TC_BODY(run_params_init__defaults, tc)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);

    ATF_REQUIRE_EQ(60, run_params.timeout_seconds);
    ATF_REQUIRE_EQ(getuid(), run_params.unprivileged_user);
    ATF_REQUIRE_EQ(getgid(), run_params.unprivileged_group);
    ATF_REQUIRE_STREQ(".", run_params.work_directory);
}


ATF_TC_WITHOUT_HEAD(fork_exec_wait__ok);
ATF_TC_BODY(fork_exec_wait__ok, tc)
{
    const char* const args[] = {"sh", "-c", "exit 42", NULL};
    int exitstatus = -1;  // Shut up GCC warning.
    const kyua_error_t error = exec_check("/bin/sh", args, &exitstatus);
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_EQ(42, exitstatus);
}


ATF_TC(fork_exec_wait__eacces);
ATF_TC_HEAD(fork_exec_wait__eacces, tc)
{
    atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fork_exec_wait__eacces, tc)
{
    ATF_REQUIRE(mkdir("dir", 0000) != -1);

    const char* const args[] = {"foo", NULL};
    int unused_exitstatus;
    const kyua_error_t error = exec_check("./dir/foo", args,
                                          &unused_exitstatus);
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    ATF_REQUIRE_EQ(EACCES, kyua_libc_error_errno(error));
}


ATF_TC_WITHOUT_HEAD(fork_exec_wait__enoent);
ATF_TC_BODY(fork_exec_wait__enoent, tc)
{
    const char* const args[] = {"foo", NULL};
    int unused_exitstatus;
    const kyua_error_t error = exec_check("./foo", args, &unused_exitstatus);
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    ATF_REQUIRE_EQ(ENOENT, kyua_libc_error_errno(error));
}


ATF_TC_WITHOUT_HEAD(fork_wait__core_size);
ATF_TC_BODY(fork_wait__core_size, tc)
{
#if !defined(__minix)
    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) == -1)
        atf_tc_skip("Failed to lower the core size limit");
#endif /* !defined(__minix) */

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);

    pid_t pid;
    kyua_error_t error = kyua_run_fork(&run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0)
        abort();

    ATF_REQUIRE(!kyua_error_is_set(error));
    int status; bool timed_out;
    error = kyua_run_wait(pid, &status, &timed_out);
    if (kyua_error_is_set(error))
        atf_tc_fail("wait failed; unexpected problem during exec?");

    ATF_REQUIRE(!timed_out);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE_MSG(WCOREDUMP(status), "Core not dumped as expected");
}


ATF_TC_WITHOUT_HEAD(fork_wait__env);
ATF_TC_BODY(fork_wait__env, tc)
{
    kyua_env_set("HOME", "/non-existent/directory");
    kyua_env_set("LANG", "C");
    kyua_env_set("LC_ALL", "C");
    kyua_env_set("LC_COLLATE", "C");
    kyua_env_set("LC_CTYPE", "C");
    kyua_env_set("LC_MESSAGES", "C");
    kyua_env_set("LC_MONETARY", "C");
    kyua_env_set("LC_NUMERIC", "C");
    kyua_env_set("LC_TIME", "C");
    kyua_env_set("LEAVE_ME_ALONE", "kill-some-day");
    kyua_env_set("TZ", "EST+5");

    ATF_REQUIRE_MSG(fork_check(NULL, check_env, NULL),
                    "Unclean environment in subprocess");
}


ATF_TC_WITHOUT_HEAD(fork_wait__process_group);
ATF_TC_BODY(fork_wait__process_group, tc)
{
    ATF_REQUIRE_MSG(fork_check(NULL, check_process_group, NULL),
                    "Subprocess not in its own process group");
}


ATF_TC_WITHOUT_HEAD(fork_wait__signals);
ATF_TC_BODY(fork_wait__signals, tc)
{
    ATF_REQUIRE_MSG(LAST_SIGNO > 10, "LAST_SIGNO as detected by configure is "
                    "suspiciously low");

    int signo;
    for (signo = 1; signo <= LAST_SIGNO; signo++) {
        if (signo == SIGKILL || signo == SIGSTOP) {
            // Ignore immutable signals.
            continue;
        }
        if (signo == SIGCHLD) {
            // If we were to reset SIGCHLD to SIG_IGN (which is different than
            // not touching the signal at all, leaving it at its default value),
            // our child process will not become a zombie and the call to
            // kyua_run_wait will fail.  Avoid this.
            continue;
        }

        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        printf("Ignoring signal %d\n", signo);
        ATF_REQUIRE(sigaction(signo, &sa, NULL) != -1);
    }

    ATF_REQUIRE_MSG(fork_check(NULL, check_signals, NULL),
                    "Signals not reset to their default state");
}


ATF_TC_WITHOUT_HEAD(fork_wait__timeout);
ATF_TC_BODY(fork_wait__timeout, tc)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.timeout_seconds = 1;

    pid_t pid;
    kyua_error_t error = kyua_run_fork(&run_params, &pid);
    if (!kyua_error_is_set(error) && pid == 0) {
        sigset_t mask;
        sigemptyset(&mask);
        for (;;)
            sigsuspend(&mask);
    }
    ATF_REQUIRE(!kyua_error_is_set(error));
    int status; bool timed_out;
    kyua_run_wait(pid, &status, &timed_out);
    ATF_REQUIRE(timed_out);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE_EQ(SIGKILL, WTERMSIG(status));
}


ATF_TC_WITHOUT_HEAD(fork_wait__umask);
ATF_TC_BODY(fork_wait__umask, tc)
{
    (void)umask(0222);
    ATF_REQUIRE_MSG(fork_check(NULL, check_umask, NULL),
                    "Subprocess does not have the predetermined 0022 umask");
}


ATF_TC(fork_wait__unprivileged_user);
ATF_TC_HEAD(fork_wait__unprivileged_user, tc)
{
    atf_tc_set_md_var(tc, "require.config", "unprivileged-user");
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(fork_wait__unprivileged_user, tc)
{
    const struct passwd* pw = getpwnam(atf_tc_get_config_var(
        tc, "unprivileged-user"));
    ATF_REQUIRE_MSG(pw != NULL, "Cannot find unprivileged user");

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.unprivileged_user = pw->pw_uid;

    ATF_REQUIRE_MSG(fork_check(&run_params, check_uid_not_root, NULL),
                    "Subprocess is still running with UID set to root");
}


ATF_TC(fork_wait__unprivileged_group);
ATF_TC_HEAD(fork_wait__unprivileged_group, tc)
{
    atf_tc_set_md_var(tc, "require.config", "unprivileged-user");
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(fork_wait__unprivileged_group, tc)
{
    const struct passwd* pw = getpwnam(atf_tc_get_config_var(
        tc, "unprivileged-user"));
    ATF_REQUIRE_MSG(pw != NULL, "Cannot find unprivileged user");

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.unprivileged_group = pw->pw_gid;

    ATF_REQUIRE_MSG(fork_check(&run_params, check_gid_not_root, NULL),
                    "Subprocess is still running with GID set to root");
}


ATF_TC(fork_wait__unprivileged_both);
ATF_TC_HEAD(fork_wait__unprivileged_both, tc)
{
    atf_tc_set_md_var(tc, "require.config", "unprivileged-user");
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(fork_wait__unprivileged_both, tc)
{
    const struct passwd* pw = getpwnam(atf_tc_get_config_var(
        tc, "unprivileged-user"));
    ATF_REQUIRE_MSG(pw != NULL, "Cannot find unprivileged user");

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.unprivileged_user = pw->pw_uid;
    run_params.unprivileged_group = pw->pw_gid;

    ATF_REQUIRE_MSG(fork_check(&run_params, check_not_root, NULL),
                    "Subprocess is still running with root privileges");
}


ATF_TC_WITHOUT_HEAD(fork_wait__work_directory);
ATF_TC_BODY(fork_wait__work_directory, tc)
{
    ATF_REQUIRE(mkdir("the-work-directory", 0755) != -1);
    atf_utils_create_file("the-work-directory/data-file", "%s", "");

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.work_directory = "./the-work-directory";
    ATF_REQUIRE_MSG(fork_check(&run_params, check_work_directory, "data-file"),
                    "Subprocess not in its own process group");
}


ATF_TC_WITHOUT_HEAD(work_directory__builtin_tmpdir);
ATF_TC_BODY(work_directory__builtin_tmpdir, tc)
{
    char* tmpdir;
    RE(kyua_fs_make_absolute("worktest", &tmpdir));
    ATF_REQUIRE(mkdir(tmpdir, 0755) != -1);
    RE(kyua_env_unset("TMPDIR"));
    kyua_run_tmpdir = tmpdir;

    char* work_directory;
    RE(kyua_run_work_directory_enter("template.XXXXXX", getuid(), getgid(),
                                     &work_directory));

    {
        char* template_test;
        RE(kyua_fs_concat(&template_test, atf_tc_get_config_var(tc, "srcdir"),
                          "worktest", "template.XXXXXX", NULL));
        ATF_REQUIRE(access(template_test, X_OK) == -1);
        free(template_test);
    }

    ATF_REQUIRE(access(work_directory, X_OK) != -1);

    ATF_REQUIRE(rmdir(tmpdir) == -1);  // Not yet empty.
    RE(kyua_run_work_directory_leave(&work_directory));
    ATF_REQUIRE(rmdir(tmpdir) != -1);
    free(tmpdir);
}


ATF_TC_WITHOUT_HEAD(work_directory__env_tmpdir);
ATF_TC_BODY(work_directory__env_tmpdir, tc)
{
    char* tmpdir;
    RE(kyua_fs_make_absolute("worktest", &tmpdir));
    ATF_REQUIRE(mkdir(tmpdir, 0755) != -1);
    RE(kyua_env_set("TMPDIR", tmpdir));

    char* work_directory;
    RE(kyua_run_work_directory_enter("template.XXXXXX", getuid(), getgid(),
                                     &work_directory));

    {
        char* template_test;
        RE(kyua_fs_concat(&template_test, atf_tc_get_config_var(tc, "srcdir"),
                          "worktest", "template.XXXXXX", NULL));
        ATF_REQUIRE(access(template_test, X_OK) == -1);
        free(template_test);
    }

    ATF_REQUIRE(access(work_directory, X_OK) != -1);

    ATF_REQUIRE(rmdir(tmpdir) == -1);  // Not yet empty.
    RE(kyua_run_work_directory_leave(&work_directory));
    ATF_REQUIRE(rmdir(tmpdir) != -1);
    free(tmpdir);
}


ATF_TC(work_directory__permissions);
ATF_TC_HEAD(work_directory__permissions, tc)
{
    atf_tc_set_md_var(tc, "require.config", "unprivileged-user");
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(work_directory__permissions, tc)
{
    const struct passwd* pw = getpwnam(atf_tc_get_config_var(
        tc, "unprivileged-user"));

    printf("%d %d %d %d\n", getuid(), getgid(), pw->pw_uid, pw->pw_gid);

    char* work_directory;
    RE(kyua_run_work_directory_enter("template.XXXXXX", pw->pw_uid, pw->pw_gid,
                                     &work_directory));

    struct stat sb;
    ATF_REQUIRE(stat(work_directory, &sb) != -1);
    ATF_REQUIRE_EQ(pw->pw_uid, sb.st_uid);
    ATF_REQUIRE_EQ(pw->pw_gid, sb.st_gid);

    RE(kyua_run_work_directory_leave(&work_directory));
}


ATF_TC(work_directory__mkdtemp_error);
ATF_TC_HEAD(work_directory__mkdtemp_error, tc)
{
    atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(work_directory__mkdtemp_error, tc)
{
    char* tmpdir;
    RE(kyua_fs_make_absolute("worktest", &tmpdir));
    ATF_REQUIRE(mkdir(tmpdir, 0555) != -1);
    RE(kyua_env_set("TMPDIR", tmpdir));

    char* work_directory;
    const kyua_error_t error = kyua_run_work_directory_enter(
        "template.XXXXXX", getuid(), getgid(), &work_directory);
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    ATF_REQUIRE_EQ(EACCES, kyua_libc_error_errno(error));
    kyua_error_free(error);

    ATF_REQUIRE(rmdir(tmpdir) != -1);  // Empty; subdirectory not created.
    free(tmpdir);
}


ATF_TC(work_directory__permissions_error);
ATF_TC_HEAD(work_directory__permissions_error, tc)
{
    atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(work_directory__permissions_error, tc)
{
    char* tmpdir;
    RE(kyua_fs_make_absolute("worktest", &tmpdir));
    ATF_REQUIRE(mkdir(tmpdir, 0755) != -1);
    RE(kyua_env_set("TMPDIR", tmpdir));

    char* work_directory;
    const kyua_error_t error = kyua_run_work_directory_enter(
        "template.XXXXXX", getuid() + 1, getgid(), &work_directory);
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    ATF_REQUIRE_EQ(EPERM, kyua_libc_error_errno(error));
    kyua_error_free(error);

    ATF_REQUIRE(rmdir(tmpdir) != -1);  // Empty; subdirectory not created.
    free(tmpdir);
}


/// Performs a signal delivery test to the work directory handling code.
///
/// \param signo The signal to deliver.
static void
work_directory_signal_check(const int signo)
{
    char* tmpdir;
    RE(kyua_fs_make_absolute("worktest", &tmpdir));
    ATF_REQUIRE(mkdir(tmpdir, 0755) != -1);
    RE(kyua_env_set("TMPDIR", tmpdir));

    char* work_directory;
    RE(kyua_run_work_directory_enter("template.XXXXXX", getuid(), getgid(),
                                     &work_directory));

    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);
    run_params.work_directory = work_directory;

    pid_t pid;
    RE(kyua_run_fork(&run_params, &pid));
    if (pid == 0) {
        sleep(run_params.timeout_seconds * 2);
        abort();
    }

    // This should cause the handled installed by the work_directory management
    // code to terminate the subprocess so that we get a chance to run the
    // cleanup code ourselves.
    kill(getpid(), signo);

    int status; bool timed_out;
    RE(kyua_run_wait(pid, &status, &timed_out));
    ATF_REQUIRE(!timed_out);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE_EQ(SIGKILL, WTERMSIG(status));

    ATF_REQUIRE(rmdir(tmpdir) == -1);  // Not yet empty.
    RE(kyua_run_work_directory_leave(&work_directory));
    ATF_REQUIRE(rmdir(tmpdir) != -1);
    free(tmpdir);
}


ATF_TC_WITHOUT_HEAD(work_directory__sighup);
ATF_TC_BODY(work_directory__sighup, tc)
{
    work_directory_signal_check(SIGHUP);
}


ATF_TC_WITHOUT_HEAD(work_directory__sigint);
ATF_TC_BODY(work_directory__sigint, tc)
{
    work_directory_signal_check(SIGINT);
}


ATF_TC_WITHOUT_HEAD(work_directory__sigterm);
ATF_TC_BODY(work_directory__sigterm, tc)
{
    work_directory_signal_check(SIGTERM);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, run_params_init__defaults);

    ATF_TP_ADD_TC(tp, fork_exec_wait__ok);
    ATF_TP_ADD_TC(tp, fork_exec_wait__eacces);
    ATF_TP_ADD_TC(tp, fork_exec_wait__enoent);

    ATF_TP_ADD_TC(tp, fork_wait__core_size);
    ATF_TP_ADD_TC(tp, fork_wait__env);
    ATF_TP_ADD_TC(tp, fork_wait__process_group);
    ATF_TP_ADD_TC(tp, fork_wait__signals);
    ATF_TP_ADD_TC(tp, fork_wait__timeout);
    ATF_TP_ADD_TC(tp, fork_wait__umask);
    ATF_TP_ADD_TC(tp, fork_wait__unprivileged_user);
    ATF_TP_ADD_TC(tp, fork_wait__unprivileged_group);
    ATF_TP_ADD_TC(tp, fork_wait__unprivileged_both);
    ATF_TP_ADD_TC(tp, fork_wait__work_directory);

    ATF_TP_ADD_TC(tp, work_directory__builtin_tmpdir);
    ATF_TP_ADD_TC(tp, work_directory__env_tmpdir);
    ATF_TP_ADD_TC(tp, work_directory__permissions);
    ATF_TP_ADD_TC(tp, work_directory__permissions_error);
    ATF_TP_ADD_TC(tp, work_directory__mkdtemp_error);
    ATF_TP_ADD_TC(tp, work_directory__sighup);
    ATF_TP_ADD_TC(tp, work_directory__sigint);
    ATF_TP_ADD_TC(tp, work_directory__sigterm);

    return atf_no_error();
}
