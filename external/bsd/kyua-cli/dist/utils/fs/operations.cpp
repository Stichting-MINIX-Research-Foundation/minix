// Copyright 2010 Google Inc.
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

#include "utils/fs/operations.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "utils/auto_array.ipp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;

using utils::optional;


namespace {


/// Stats a file, without following links.
///
/// \param path The file to stat.
///
/// \return The stat structure on success.
///
/// \throw system_error An error on failure.
static struct ::stat
safe_stat(const fs::path& path)
{
    struct ::stat sb;
    if (::lstat(path.c_str(), &sb) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot get information about %s") % path,
                               original_errno);
    }
    return sb;
}


}  // anonymous namespace


/// Queries the path to the current directory.
///
/// \return The path to the current directory.
///
/// \throw fs::error If there is a problem querying the current directory.
fs::path
fs::current_path(void)
{
    char* cwd;
#if defined(HAVE_GETCWD_DYN)
    cwd = ::getcwd(NULL, 0);
#else
    cwd = ::getcwd(NULL, MAXPATHLEN);
#endif
    if (cwd == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to get current working directory"),
                               original_errno);
    }

    try {
        const fs::path result(cwd);
        std::free(cwd);
        return result;
    } catch (...) {
        std::free(cwd);
        throw;
    }
}


/// Checks if a file exists.
///
/// Be aware that this is racy in the same way as access(2) is.
///
/// \param path The file to check the existance of.
///
/// \return True if the file exists; false otherwise.
bool
fs::exists(const fs::path& path)
{
    return ::access(path.c_str(), F_OK) == 0;
}


/// Locates a file in the PATH.
///
/// \param name The file to locate.
///
/// \return The path to the located file or none if it was not found.  The
/// returned path is always absolute.
optional< fs::path >
fs::find_in_path(const char* name)
{
    const optional< std::string > current_path = utils::getenv("PATH");
    if (!current_path || current_path.get().empty())
        return none;

    std::istringstream path_input(current_path.get() + ":");
    std::string path_component;
    while (std::getline(path_input, path_component, ':').good()) {
        const fs::path candidate = path_component.empty() ?
            fs::path(name) : (fs::path(path_component) / name);
        if (exists(candidate)) {
            if (candidate.is_absolute())
                return utils::make_optional(candidate);
            else
                return utils::make_optional(candidate.to_absolute());
        }
    }
    return none;
}


/// Creates a directory.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directory.
///
/// \throw system_error If the call to mkdir(2) fails.
void
fs::mkdir(const fs::path& dir, const int mode)
{
    if (::mkdir(dir.c_str(), static_cast< mode_t >(mode)) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to create directory %s") % dir,
                               original_errno);
    }
}


/// Creates a directory and any missing parents.
///
/// This is separate from the fs::mkdir function to clearly differentiate the
/// libc wrapper from the more complex algorithm implemented here.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directories.
///
/// \throw system_error If any call to mkdir(2) fails.
void
fs::mkdir_p(const fs::path& dir, const int mode)
{
    try {
        fs::mkdir(dir, mode);
    } catch (const fs::system_error& e) {
        if (e.original_errno() == ENOENT) {
            fs::mkdir_p(dir.branch_path(), mode);
            fs::mkdir(dir, mode);
        } else if (e.original_errno() != EEXIST)
            throw e;
    }
}


/// Creates a temporary directory.
///
/// The temporary directory is created using mkdtemp(3) using the provided
/// template.  This should be most likely used in conjunction with
/// fs::auto_directory.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The generated path for the temporary directory.
///
/// \throw fs::system_error If the call to mkdtemp(3) fails.
fs::path
fs::mkdtemp(const std::string& path_template)
{
    PRE(path_template.find("XXXXXX") != std::string::npos);

    const fs::path tmpdir(utils::getenv_with_default("TMPDIR", "/tmp"));
    const fs::path full_template = tmpdir / path_template;

    utils::auto_array< char > buf(new char[full_template.str().length() + 1]);
    std::strcpy(buf.get(), full_template.c_str());
    if (::mkdtemp(buf.get()) == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot create temporary directory using "
                                 "template %s") % full_template,
                               original_errno);
    }
    return fs::path(buf.get());
}


/// Creates a temporary file.
///
/// The temporary file is created using mkstemp(3) using the provided template.
/// This should be most likely used in conjunction with fs::auto_file.
///
/// \param path_template The template for the temporary path, which is a
///     basename that is created within the TMPDIR.  Must contain the XXXXXX
///     pattern, which is atomically replaced by a random unique string.
///
/// \return The generated path for the temporary directory.
///
/// \throw fs::system_error If the call to mkstemp(3) fails.
fs::path
fs::mkstemp(const std::string& path_template)
{
    PRE(path_template.find("XXXXXX") != std::string::npos);

    const fs::path tmpdir(utils::getenv_with_default("TMPDIR", "/tmp"));
    const fs::path full_template = tmpdir / path_template;

    utils::auto_array< char > buf(new char[full_template.str().length() + 1]);
    std::strcpy(buf.get(), full_template.c_str());
    if (::mkstemp(buf.get()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot create temporary file using template "
                                 "%s") % full_template, original_errno);
    }
    return fs::path(buf.get());
}


/// Recursively removes a directory.
///
/// This operation simulates a "rm -r".  No effort is made to forcibly delete
/// files and no attention is paid to mount points.
///
/// \param directory The directory to remove.
///
/// \throw fs::error If there is a problem removing any directory or file.
void
fs::rm_r(const fs::path& directory)
{
    DIR* dirp = ::opendir(directory.c_str());
    if (dirp == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to open directory %s") %
                               directory.str(), original_errno);
    }
    try {
        ::dirent* dp;
        while ((dp = ::readdir(dirp)) != NULL) {
            const std::string name = dp->d_name;
            if (name == "." || name == "..")
                continue;

            const fs::path entry = directory / dp->d_name;

            const struct ::stat sb = safe_stat(entry);
            if (S_ISDIR(sb.st_mode)) {
                LD(F("Descending into %s") % entry);
                fs::rm_r(entry);
            } else {
                LD(F("Removing file %s") % entry);
                fs::unlink(entry);
            }
        }
    } catch (...) {
        ::closedir(dirp);
        throw;
    }
    ::closedir(dirp);

    LD(F("Removing empty directory %s") % directory);
    fs::rmdir(directory);
}


/// Removes an empty directory.
///
/// \param file The directory to remove.
///
/// \throw fs::system_error If the call to rmdir(2) fails.
void
fs::rmdir(const path& file)
{
    if (::rmdir(file.c_str()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Removal of %s failed") % file,
                               original_errno);
    }
}


/// Removes a file.
///
/// \param file The file to remove.
///
/// \throw fs::system_error If the call to unlink(2) fails.
void
fs::unlink(const path& file)
{
    if (::unlink(file.c_str()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Removal of %s failed") % file,
                               original_errno);
    }
}
