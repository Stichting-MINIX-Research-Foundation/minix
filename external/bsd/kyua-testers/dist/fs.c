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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#if defined(HAVE_UNMOUNT)
#   include <sys/param.h>
#   include <sys/mount.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "error.h"


/// Specifies if a real unmount(2) is available.
///
/// We use this as a constant instead of a macro so that we can compile both
/// versions of the unmount code unconditionally.  This is a way to prevent
/// compilation bugs going unnoticed for long.
static const bool have_unmount2 =
#if defined(HAVE_UNMOUNT)
    true;
#else
    false;
#endif


#if !defined(UMOUNT)
/// Fake replacement value to the path to umount(8).
#   define UMOUNT "do-not-use-this-value"
#else
#   if defined(HAVE_UNMOUNT)
#       error "umount(8) detected when unmount(2) is also available"
#   endif
#endif


#if !defined(HAVE_UNMOUNT)
/// Fake unmount(2) function for systems without it.
///
/// This is only provided to allow our code to compile in all platforms
/// regardless of whether they actually have an unmount(2) or not.
///
/// \param unused_path The mount point to be unmounted.
/// \param unused_flags The flags to the unmount(2) call.
///
/// \return -1 to indicate error, although this should never happen.
static int
unmount(const char* KYUA_DEFS_UNUSED_PARAM(path),
        const int KYUA_DEFS_UNUSED_PARAM(flags))
{
    assert(false);
    return -1;
}
#endif


/// Scans a directory and executes a callback on each entry.
///
/// \param directory The directory to scan.
/// \param callback The function to execute on each entry.
/// \param argument A cookie to pass to the callback function.
///
/// \return True if the directory scan and the calls to the callback function
/// are all successful; false otherwise.
///
/// \note Errors are logged to stderr and do not stop the algorithm.
static bool
try_iterate_directory(const char* directory,
                      bool (*callback)(const char*, const void*),
                      const void* argument)
{
    bool ok = true;

    DIR* dirp = opendir(directory);
    if (dirp == NULL) {
        warn("opendir(%s) failed", directory);
        ok &= false;
    } else {
        struct dirent* dp;
        while ((dp = readdir(dirp)) != NULL) {
            const char* name = dp->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            char* subdir;
            const kyua_error_t error = kyua_fs_concat(&subdir, directory, name,
                                                      NULL);
            if (kyua_error_is_set(error)) {
                kyua_error_free(error);
                warn("path concatenation failed");
                ok &= false;
            } else {
                ok &= callback(subdir, argument);
                free(subdir);
            }
        }
        closedir(dirp);
    }

    return ok;
}


/// Stats a file, without following links.
///
/// \param path The file to stat.
/// \param [out] sb Pointer to the stat structure in which to place the result.
///
/// \return The stat structure on success; none on failure.
///
/// \note Errors are logged to stderr.
static bool
try_stat(const char* path, struct stat* sb)
{
    if (lstat(path, sb) == -1) {
        warn("lstat(%s) failed", path);
        return false;
    } else
        return true;
}


/// Removes a directory.
///
/// \param path The directory to remove.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr.
static bool
try_rmdir(const char* path)
{
    if (rmdir(path) == -1) {
        warn("rmdir(%s) failed", path);
        return false;
    } else
        return true;
}


/// Removes a file.
///
/// \param path The file to remove.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr.
static bool
try_unlink(const char* path)
{
    if (unlink(path) == -1) {
        warn("unlink(%s) failed", path);
        return false;
    } else
        return true;
}


/// Unmounts a mount point.
///
/// \param path The location to unmount.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr.
static bool
try_unmount(const char* path)
{
    const kyua_error_t error = kyua_fs_unmount(path);
    if (kyua_error_is_set(error)) {
        kyua_error_warn(error, "Cannot unmount %s", path);
        kyua_error_free(error);
        return false;
    } else
        return true;
}


/// Attempts to weaken the permissions of a file.
///
/// \param path The file to unprotect.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr.
static bool
try_unprotect(const char* path)
{
    static const mode_t new_mode = 0700;

    if (chmod(path, new_mode) == -1) {
        warnx("chmod(%s, %04o) failed", path, new_mode);
        return false;
    } else
        return true;
}


/// Attempts to weaken the permissions of a symbolic link.
///
/// \param path The symbolic link to unprotect.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr.
static bool
try_unprotect_symlink(const char* path)
{
    static const mode_t new_mode = 0700;

#if HAVE_WORKING_LCHMOD
    if (lchmod(path, new_mode) == -1) {
        warnx("lchmod(%s, %04o) failed", path, new_mode);
        return false;
    } else
        return true;
#else
    warnx("lchmod(%s, %04o) failed; system call not implemented", path,
          new_mode);
    return false;
#endif
}


/// Traverses a hierarchy unmounting any mount points in it.
///
/// \param current_path The file or directory to traverse.
/// \param raw_parent_sb The stat structure of the enclosing directory.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr and do not stop the algorithm.
static bool
recursive_unmount(const char* current_path, const void* raw_parent_sb)
{
    const struct stat* parent_sb = raw_parent_sb;

    struct stat current_sb;
    bool ok = try_stat(current_path, &current_sb);
    if (ok) {
        if (S_ISDIR(current_sb.st_mode)) {
            assert(!S_ISLNK(current_sb.st_mode));
            ok &= try_iterate_directory(current_path, recursive_unmount,
                                        &current_sb);
        }

        if (current_sb.st_dev != parent_sb->st_dev)
            ok &= try_unmount(current_path);
    }

    return ok;
}


/// Traverses a hierarchy and removes all of its contents.
///
/// This honors mount points: when a mount point is encountered, it is traversed
/// in search for other mount points, but no files within any of these are
/// removed.
///
/// \param current_path The file or directory to traverse.
/// \param raw_parent_sb The stat structure of the enclosing directory.
///
/// \return True on success; false otherwise.
///
/// \note Errors are logged to stderr and do not stop the algorithm.
static bool
recursive_cleanup(const char* current_path, const void* raw_parent_sb)
{
    const struct stat* parent_sb = raw_parent_sb;

    struct stat current_sb;
    bool ok = try_stat(current_path, &current_sb);
    if (ok) {
        // Weakening the protections of a file is just a best-effort operation.
        // If this fails, we may still be able to do the file/directory removal
        // later on, so ignore any failures from try_unprotect().
        //
        // One particular case in which this fails is if try_unprotect() is run
        // on a symbolic link that points to a file for which the unprotect is
        // not possible, and lchmod(3) is not available.
        if (S_ISLNK(current_sb.st_mode))
            try_unprotect_symlink(current_path);
        else
            try_unprotect(current_path);

        if (current_sb.st_dev != parent_sb->st_dev) {
            ok &= recursive_unmount(current_path, parent_sb);
            if (ok)
                ok &= recursive_cleanup(current_path, parent_sb);
        } else {
            if (S_ISDIR(current_sb.st_mode)) {
                assert(!S_ISLNK(current_sb.st_mode));
                ok &= try_iterate_directory(current_path, recursive_cleanup,
                                            &current_sb);
                ok &= try_rmdir(current_path);
            } else {
                ok &= try_unlink(current_path);
            }
        }
    }

    return ok;
}


/// Unmounts a file system using unmount(2).
///
/// \pre unmount(2) must be available; i.e. have_unmount2 must be true.
///
/// \param mount_point The file system to unmount.
///
/// \return An error object.
static kyua_error_t
unmount_with_unmount2(const char* mount_point)
{
    assert(have_unmount2);

    if (unmount(mount_point, 0) == -1) {
        return kyua_libc_error_new(errno, "unmount(%s) failed",
                                   mount_point);
    }

    return kyua_error_ok();
}


/// Unmounts a file system using umount(8).
///
/// \pre umount(2) must not be available; i.e. have_unmount2 must be false.
///
/// \param mount_point The file system to unmount.
///
/// \return An error object.
static kyua_error_t
unmount_with_umount8(const char* mount_point)
{
    assert(!have_unmount2);

    const pid_t pid = fork();
    if (pid == -1) {
        return kyua_libc_error_new(errno, "fork() failed");
    } else if (pid == 0) {
        const int ret = execlp(UMOUNT, "umount", mount_point, NULL);
        assert(ret == -1);
        err(EXIT_FAILURE, "Failed to execute " UMOUNT);
    }

    kyua_error_t error = kyua_error_ok();
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        error = kyua_libc_error_new(errno, "waitpid(%d) failed", pid);
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EXIT_SUCCESS)
                assert(!kyua_error_is_set(error));
            else {
                error = kyua_libc_error_new(EBUSY, "unmount(%s) failed",
                                            mount_point);
            }
        } else
            error = kyua_libc_error_new(EFAULT, "umount(8) crashed");
    }
    return error;
}


/// Recursively removes a directory.
///
/// \param root The directory or file to remove.  Cannot be a mount point.
///
/// \return An error object.
kyua_error_t
kyua_fs_cleanup(const char* root)
{
    struct stat current_sb;
    bool ok = try_stat(root, &current_sb);
    if (ok)
        ok &= recursive_cleanup(root, &current_sb);

    if (!ok) {
        warnx("Cleanup of '%s' failed", root);
        return kyua_libc_error_new(EPERM, "Cleanup of %s failed", root);
    } else
        return kyua_error_ok();
}


/// Concatenates a set of strings to form a path.
///
/// \param [out] output Pointer to a dynamically-allocated string that will hold
///     the resulting path, if all goes well.
/// \param first First component of the path to concatenate.
/// \param ... All other components to concatenate.
///
/// \return An error if there is not enough memory to fulfill the request; OK
/// otherwise.
kyua_error_t
kyua_fs_concat(char** const output, const char* first, ...)
{
    va_list ap;
    const char* component;

    va_start(ap, first);
    size_t length = strlen(first) + 1;
    while ((component = va_arg(ap, const char*)) != NULL) {
        length += 1 + strlen(component);
    }
    va_end(ap);

    *output = (char*)malloc(length);
    if (output == NULL)
        return kyua_oom_error_new();
    char* iterator = *output;

    int added_size;
    added_size = snprintf(iterator, length, "%s", first);
    iterator += added_size; length -= added_size;

    va_start(ap, first);
    while ((component = va_arg(ap, const char*)) != NULL) {
        added_size = snprintf(iterator, length, "/%s", component);
        iterator += added_size; length -= added_size;
    }
    va_end(ap);

    return kyua_error_ok();
}


/// Queries the path to the current directory.
///
/// \param [out] out_cwd Dynamically-allocated pointer to a string holding the
///     current path.  The caller must use free() to release it.
///
/// \return An error object.
kyua_error_t
kyua_fs_current_path(char** out_cwd)
{
    char* cwd;
#if defined(HAVE_GETCWD_DYN)
    cwd = getcwd(NULL, 0);
#else
    {
        const char* static_cwd = ::getcwd(NULL, MAXPATHLEN);
        const kyua_error_t error = kyua_fs_concat(&cwd, static_cwd, NULL);
        if (kyua_error_is_set(error))
            return error;
    }
#endif
    if (cwd == NULL) {
        return kyua_libc_error_new(errno, "getcwd() failed");
    } else {
        *out_cwd = cwd;
        return kyua_error_ok();
    }
}


/// Converts a path to absolute.
///
/// \param original The path to convert; may already be absolute.
/// \param [out] output Pointer to a dynamically-allocated string that will hold
///     the absolute path, if all goes well.
///
/// \return An error if there is not enough memory to fulfill the request; OK
/// otherwise.
kyua_error_t
kyua_fs_make_absolute(const char* original, char** const output)
{
    if (original[0] == '/') {
        *output = (char*)malloc(strlen(original) + 1);
        if (output == NULL)
            return kyua_oom_error_new();
        strcpy(*output, original);
        return kyua_error_ok();
    } else {
        char* current_path;
        kyua_error_t error;

        error = kyua_fs_current_path(&current_path);
        if (kyua_error_is_set(error))
            return error;

        error = kyua_fs_concat(output, current_path, original, NULL);
        free(current_path);
        return error;
    }
}


/// Unmounts a file system.
///
/// \param mount_point The file system to unmount.
///
/// \return An error object.
kyua_error_t
kyua_fs_unmount(const char* mount_point)
{
    kyua_error_t error;

    // FreeBSD's unmount(2) requires paths to be absolute.  To err on the side
    // of caution, let's make it absolute in all cases.
    char* abs_mount_point;
    error = kyua_fs_make_absolute(mount_point, &abs_mount_point);
    if (kyua_error_is_set(error))
        goto out;

    static const int unmount_retries = 3;
    static const int unmount_retry_delay_seconds = 1;

    int retries = unmount_retries;
retry:
    if (have_unmount2) {
        error = unmount_with_unmount2(abs_mount_point);
    } else {
        error = unmount_with_umount8(abs_mount_point);
    }
    if (kyua_error_is_set(error)) {
        assert(kyua_error_is_type(error, "libc"));
        if (kyua_libc_error_errno(error) == EBUSY && retries > 0) {
            kyua_error_warn(error, "%s busy; unmount retries left %d",
                            abs_mount_point, retries);
            kyua_error_free(error);
            retries--;
            sleep(unmount_retry_delay_seconds);
            goto retry;
        }
    }

out:
    free(abs_mount_point);
    return error;
}
