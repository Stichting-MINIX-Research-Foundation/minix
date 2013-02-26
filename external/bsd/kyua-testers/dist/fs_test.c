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

#include "fs.h"

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "error.h"


static void run_mount_tmpfs(const char*) KYUA_DEFS_NORETURN;


/// Operating systems recognized by the code below.
enum os_type {
    os_unsupported = 0,
    os_freebsd,
    os_linux,
    os_netbsd,
    os_sunos,
};


/// The current operating system.
static enum os_type current_os =
#if defined(__FreeBSD__)
    os_freebsd
#elif defined(__linux__)
    os_linux
#elif defined(__NetBSD__)
    os_netbsd
#elif defined(__SunOS__)
    os_sunos
#else
    os_unsupported
#endif
    ;


/// Checks if a directory entry exists and matches a specific type.
///
/// \param dir The directory in which to look for the entry.
/// \param name The name of the entry to look up.
/// \param expected_type The expected type of the file as given by dir(5).
///
/// \return True if the entry exists and matches the given type; false
/// otherwise.
static bool
lookup(const char* dir, const char* name, const int expected_type)
{
    DIR* dirp = opendir(dir);
    ATF_REQUIRE(dirp != NULL);

    bool found = false;
    struct dirent* dp;
    while (!found && (dp = readdir(dirp)) != NULL) {
        if (strcmp(dp->d_name, name) == 0 &&
            dp->d_type == expected_type) {
            found = true;
        }
    }
    closedir(dirp);
    return found;
}


/// Executes 'mount -t tmpfs' (or a similar variant).
///
/// This function must be called from a subprocess, as it never returns.
///
/// \param mount_point Location on which to mount a tmpfs.
static void
run_mount_tmpfs(const char* mount_point)
{
    const char* mount_args[16];

    size_t last = 0;
    switch (current_os) {
    case os_freebsd:
        mount_args[last++] = "mdmfs";
        mount_args[last++] = "-s16m";
        mount_args[last++] = "md";
        mount_args[last++] = mount_point;
        break;

    case os_linux:
        mount_args[last++] = "mount";
        mount_args[last++] = "-ttmpfs";
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point;
        break;

    case os_netbsd:
        mount_args[last++] = "mount";
        mount_args[last++] = "-ttmpfs";
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point;
        break;

    case os_sunos:
        mount_args[last++] = "mount";
        mount_args[last++] = "-Ftmpfs";
        mount_args[last++] = "tmpfs";
        mount_args[last++] = mount_point;
        break;

    default:
        err(123, "Don't know how to mount a file system for testing "
            "purposes");
    }
    mount_args[last] = NULL;

    const char** arg;
    printf("Mounting tmpfs onto %s with:", mount_point);
    for (arg = &mount_args[0]; *arg != NULL; arg++)
        printf(" %s", *arg);
    printf("\n");

    const int ret = execvp(mount_args[0], KYUA_DEFS_UNCONST(mount_args));
    assert(ret == -1);
    err(EXIT_FAILURE, "Failed to exec %s", mount_args[0]);
};


/// Mounts a temporary file system.
///
/// This is only provided for testing purposes.  The mounted file system
/// contains no valuable data.
///
/// Note that the calling test case is skipped if the current operating system
/// is not supported.
///
/// \param mount_point The path on which the file system will be mounted.
static void
mount_tmpfs(const char* mount_point)
{
    // SunOS's mount(8) requires paths to be absolute.  To err on the side of
    // caution, let's make it absolute in all cases.
    //const fspath abs_mount_point = mount_point.is_absolute() ?
    //    mount_point : mount_point.to_absolute();

    pid_t pid = fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0)
        run_mount_tmpfs(mount_point);
    int status;
    ATF_REQUIRE(waitpid(pid, &status, 0) != -1);
    ATF_REQUIRE(WIFEXITED(status));
    if (WEXITSTATUS(status) == 123)
        atf_tc_skip("Don't know how to mount a file system for testing "
                    "purposes");
    else
        ATF_REQUIRE_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}


static bool
lchmod_fails(void)
{
    ATF_REQUIRE(mkdir("test", 0755) != -1);
    return lchmod("test", 0700) == -1 && chmod("test", 0700) != -1;
}


ATF_TC_WITHOUT_HEAD(cleanup__file);
ATF_TC_BODY(cleanup__file, tc)
{
    atf_utils_create_file("root", "%s", "");
    ATF_REQUIRE(lookup(".", "root", DT_REG));
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_REG));
}


ATF_TC_WITHOUT_HEAD(cleanup__subdir__empty);
ATF_TC_BODY(cleanup__subdir__empty, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC_WITHOUT_HEAD(cleanup__subdir__files_and_directories);
ATF_TC_BODY(cleanup__subdir__files_and_directories, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    atf_utils_create_file("root/.hidden_file", "%s", "");
    ATF_REQUIRE(mkdir("root/.hidden_dir", 0755) != -1);
    atf_utils_create_file("root/.hidden_dir/a", "%s", "");
    atf_utils_create_file("root/file", "%s", "");
    atf_utils_create_file("root/with spaces", "%s", "");
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1/dir2", 0755) != -1);
    atf_utils_create_file("root/dir1/dir2/file", "%s", "");
    ATF_REQUIRE(mkdir("root/dir1/dir3", 0755) != -1);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC_WITHOUT_HEAD(cleanup__subdir__unprotect_regular);
ATF_TC_BODY(cleanup__subdir__unprotect_regular, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1/dir2", 0755) != -1);
    atf_utils_create_file("root/dir1/dir2/file", "%s", "");
    ATF_REQUIRE(chmod("root/dir1/dir2/file", 0000) != -1);
    ATF_REQUIRE(chmod("root/dir1/dir2", 0000) != -1);
    ATF_REQUIRE(chmod("root/dir1", 0000) != -1);
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__subdir__unprotect_symlink);
ATF_TC_HEAD(cleanup__subdir__unprotect_symlink, tc)
{
    atf_tc_set_md_var(tc, "require.progs", "/bin/ls");
    // We are ensuring that chmod is not run on the target of a symlink, so
    // we cannot be root (nor we don't want to, to prevent unprotecting a
    // system file!).
    atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(cleanup__subdir__unprotect_symlink, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(symlink("/bin/ls", "root/dir1/ls") != -1);
    ATF_REQUIRE(chmod("root/dir1", 0555) != -1);
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC_WITHOUT_HEAD(cleanup__subdir__links);
ATF_TC_BODY(cleanup__subdir__links, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(symlink("../../root", "root/dir1/loop") != -1);
    ATF_REQUIRE(symlink("non-existent", "root/missing") != -1);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    kyua_error_t error = kyua_fs_cleanup("root");
    if (kyua_error_is_set(error)) {
        if (lchmod_fails())
            atf_tc_expect_fail("lchmod(2) is not implemented in your system");
        kyua_error_free(error);
        atf_tc_fail("kyua_fs_cleanup returned an error");
    }
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__mount_point__simple);
ATF_TC_HEAD(cleanup__mount_point__simple, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(cleanup__mount_point__simple, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    atf_utils_create_file("root/zz", "%s", "");
    mount_tmpfs("root/dir1");
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__mount_point__overlayed);
ATF_TC_HEAD(cleanup__mount_point__overlayed, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(cleanup__mount_point__overlayed, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    atf_utils_create_file("root/zz", "%s", "");
    mount_tmpfs("root/dir1");
    mount_tmpfs("root/dir1");
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__mount_point__nested);
ATF_TC_HEAD(cleanup__mount_point__nested, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(cleanup__mount_point__nested, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1/dir2", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir3", 0755) != -1);
    mount_tmpfs("root/dir1/dir2");
    mount_tmpfs("root/dir3");
    ATF_REQUIRE(mkdir("root/dir1/dir2/dir4", 0755) != -1);
    mount_tmpfs("root/dir1/dir2/dir4");
    ATF_REQUIRE(mkdir("root/dir1/dir2/not-mount-point", 0755) != -1);
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__mount_point__links);
ATF_TC_HEAD(cleanup__mount_point__links, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(cleanup__mount_point__links, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir3", 0755) != -1);
    mount_tmpfs("root/dir1");
    ATF_REQUIRE(symlink("../dir3", "root/dir1/link") != -1);
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TC(cleanup__mount_point__busy);
ATF_TC_HEAD(cleanup__mount_point__busy, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(cleanup__mount_point__busy, tc)
{
    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(mkdir("root/dir1", 0755) != -1);
    mount_tmpfs("root/dir1");

    pid_t pid = fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        if (chdir("root/dir1") == -1)
            abort();

        atf_utils_create_file("dont-delete-me", "%s", "");
        atf_utils_create_file("../../done", "%s", "");

        pause();
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "Waiting for child to finish preparations\n");
        while (!atf_utils_file_exists("done")) {}
        fprintf(stderr, "Child done; cleaning up\n");

        ATF_REQUIRE(kyua_error_is_set(kyua_fs_cleanup("root")));
        ATF_REQUIRE(atf_utils_file_exists("root/dir1/dont-delete-me"));

        fprintf(stderr, "Killing child\n");
        ATF_REQUIRE(kill(pid, SIGKILL) != -1);
        int status;
        ATF_REQUIRE(waitpid(pid, &status, 0) != -1);

        ATF_REQUIRE(!kyua_error_is_set(kyua_fs_cleanup("root")));
        ATF_REQUIRE(!lookup(".", "root", DT_DIR));
    }
}


ATF_TC_WITHOUT_HEAD(concat__one);
ATF_TC_BODY(concat__one, tc)
{
    char* path;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_concat(&path, "foo", NULL)));
    ATF_REQUIRE_STREQ("foo", path);
    free(path);
}


ATF_TC_WITHOUT_HEAD(concat__two);
ATF_TC_BODY(concat__two, tc)
{
    char* path;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_concat(&path, "foo", "bar", NULL)));
    ATF_REQUIRE_STREQ("foo/bar", path);
    free(path);
}


ATF_TC_WITHOUT_HEAD(concat__several);
ATF_TC_BODY(concat__several, tc)
{
    char* path;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_concat(&path, "/usr", ".", "bin",
                                                  "ls", NULL)));
    ATF_REQUIRE_STREQ("/usr/./bin/ls", path);
    free(path);
}


ATF_TC_WITHOUT_HEAD(current_path__ok);
ATF_TC_BODY(current_path__ok, tc)
{
    char* previous;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_current_path(&previous)));

    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(chdir("root") != -1);
    char* cwd;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_current_path(&cwd)));

    char* exp_cwd;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_concat(&exp_cwd, previous, "root",
                                                  NULL)));
    ATF_REQUIRE_STREQ(exp_cwd, cwd);

    free(exp_cwd);
    free(cwd);
    free(previous);
}


ATF_TC_WITHOUT_HEAD(current_path__enoent);
ATF_TC_BODY(current_path__enoent, tc)
{
    char* previous;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_current_path(&previous)));

    ATF_REQUIRE(mkdir("root", 0755) != -1);
    ATF_REQUIRE(chdir("root") != -1);
    ATF_REQUIRE(rmdir("../root") != -1);
    char* cwd = (char*)0xdeadbeef;
    kyua_error_t error = kyua_fs_current_path(&cwd);
    ATF_REQUIRE(kyua_error_is_set(error));
    ATF_REQUIRE(kyua_error_is_type(error, "libc"));
    ATF_REQUIRE_EQ(ENOENT, kyua_libc_error_errno(error));
    ATF_REQUIRE_EQ((char*)0xdeadbeef, cwd);
    kyua_error_free(error);

    free(previous);
}


ATF_TC_WITHOUT_HEAD(make_absolute__absolute);
ATF_TC_BODY(make_absolute__absolute, tc)
{
    char* absolute;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_make_absolute(
        "/this/is/absolute", &absolute)));
    ATF_REQUIRE_STREQ("/this/is/absolute", absolute);
    free(absolute);
}


ATF_TC_WITHOUT_HEAD(make_absolute__relative);
ATF_TC_BODY(make_absolute__relative, tc)
{
    kyua_error_t error;
    char* absolute;

    DIR* previous = opendir(".");
    ATF_REQUIRE(previous != NULL);
    ATF_REQUIRE(chdir("/usr") != -1);
    error = kyua_fs_make_absolute("bin/foobar", &absolute);
    const int previous_fd = dirfd(previous);
    ATF_REQUIRE(fchdir(previous_fd) != -1);
    close(previous_fd);

    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("/usr/bin/foobar", absolute);
    free(absolute);
}


ATF_TC(unmount__ok);
ATF_TC_HEAD(unmount__ok, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(unmount__ok, tc)
{
    ATF_REQUIRE(mkdir("mount_point", 0755) != -1);

    atf_utils_create_file("mount_point/test1", "%s", "");
    mount_tmpfs("mount_point");
    atf_utils_create_file("mount_point/test2", "%s", "");

    ATF_REQUIRE(!atf_utils_file_exists("mount_point/test1"));
    ATF_REQUIRE( atf_utils_file_exists("mount_point/test2"));
    kyua_fs_unmount("mount_point");
    ATF_REQUIRE( atf_utils_file_exists("mount_point/test1"));
    ATF_REQUIRE(!atf_utils_file_exists("mount_point/test2"));
}


ATF_TC(unmount__fail);
ATF_TC_HEAD(unmount__fail, tc)
{
    atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(unmount__fail, tc)
{
    kyua_error_t error = kyua_fs_unmount("mount_point");
    ATF_REQUIRE(kyua_error_is_set(error));
    kyua_error_free(error);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, cleanup__file);
    ATF_TP_ADD_TC(tp, cleanup__subdir__empty);
    ATF_TP_ADD_TC(tp, cleanup__subdir__files_and_directories);
    ATF_TP_ADD_TC(tp, cleanup__subdir__unprotect_regular);
    ATF_TP_ADD_TC(tp, cleanup__subdir__unprotect_symlink);
    ATF_TP_ADD_TC(tp, cleanup__subdir__links);
    ATF_TP_ADD_TC(tp, cleanup__mount_point__simple);
    ATF_TP_ADD_TC(tp, cleanup__mount_point__overlayed);
    ATF_TP_ADD_TC(tp, cleanup__mount_point__nested);
    ATF_TP_ADD_TC(tp, cleanup__mount_point__links);
    ATF_TP_ADD_TC(tp, cleanup__mount_point__busy);

    ATF_TP_ADD_TC(tp, concat__one);
    ATF_TP_ADD_TC(tp, concat__two);
    ATF_TP_ADD_TC(tp, concat__several);

    ATF_TP_ADD_TC(tp, current_path__ok);
    ATF_TP_ADD_TC(tp, current_path__enoent);

    ATF_TP_ADD_TC(tp, make_absolute__absolute);
    ATF_TP_ADD_TC(tp, make_absolute__relative);

    ATF_TP_ADD_TC(tp, unmount__ok);
    ATF_TP_ADD_TC(tp, unmount__fail);

    return atf_no_error();
}
