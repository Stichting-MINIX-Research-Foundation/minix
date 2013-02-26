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

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;

using utils::optional;


namespace {


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
    DIR* dirp = ::opendir(dir);
    ATF_REQUIRE(dirp != NULL);

    bool found = false;
    struct dirent* dp;
    while (!found && (dp = readdir(dirp)) != NULL) {
        if (std::strcmp(dp->d_name, name) == 0 &&
            dp->d_type == expected_type) {
            found = true;
        }
    }
    ::closedir(dirp);
    return found;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(current_path__ok);
ATF_TEST_CASE_BODY(current_path__ok)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    const fs::path cwd = fs::current_path();
    ATF_REQUIRE_EQ(cwd.str().length() - 5, cwd.str().find("/root"));
    ATF_REQUIRE_EQ(previous / "root", cwd);
}


ATF_TEST_CASE_WITHOUT_HEAD(current_path__enoent);
ATF_TEST_CASE_BODY(current_path__enoent)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    ATF_REQUIRE(::rmdir("../root") != -1);
    try {
        (void)fs::current_path();
        fail("system_errpr not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(exists);
ATF_TEST_CASE_BODY(exists)
{
    const fs::path dir("dir");
    ATF_REQUIRE(!fs::exists(dir));
    fs::mkdir(dir, 0755);
    ATF_REQUIRE(fs::exists(dir));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__no_path);
ATF_TEST_CASE_BODY(find_in_path__no_path)
{
    utils::unsetenv("PATH");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file("ls", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__empty_path);
ATF_TEST_CASE_BODY(find_in_path__empty_path)
{
    utils::setenv("PATH", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file("ls", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__one_component);
ATF_TEST_CASE_BODY(find_in_path__one_component)
{
    const fs::path dir = fs::current_path() / "bin";
    fs::mkdir(dir, 0755);
    utils::setenv("PATH", dir.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file((dir / "ls").str(), "");
    ATF_REQUIRE_EQ(dir / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__many_components);
ATF_TEST_CASE_BODY(find_in_path__many_components)
{
    const fs::path dir1 = fs::current_path() / "dir1";
    const fs::path dir2 = fs::current_path() / "dir2";
    fs::mkdir(dir1, 0755);
    fs::mkdir(dir2, 0755);
    utils::setenv("PATH", dir1.str() + ":" + dir2.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    atf::utils::create_file((dir2 / "ls").str(), "");
    ATF_REQUIRE_EQ(dir2 / "ls", fs::find_in_path("ls").get());
    atf::utils::create_file((dir1 / "ls").str(), "");
    ATF_REQUIRE_EQ(dir1 / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__current_directory);
ATF_TEST_CASE_BODY(find_in_path__current_directory)
{
    utils::setenv("PATH", "bin:");

    ATF_REQUIRE(!fs::find_in_path("foo-bar"));
    atf::utils::create_file("foo-bar", "");
    ATF_REQUIRE_EQ(fs::path("foo-bar").to_absolute(),
                   fs::find_in_path("foo-bar").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__always_absolute);
ATF_TEST_CASE_BODY(find_in_path__always_absolute)
{
    fs::mkdir(fs::path("my-bin"), 0755);
    utils::setenv("PATH", "my-bin");

    ATF_REQUIRE(!fs::find_in_path("abcd"));
    atf::utils::create_file("my-bin/abcd", "");
    ATF_REQUIRE_EQ(fs::path("my-bin/abcd").to_absolute(),
                   fs::find_in_path("abcd").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__ok);
ATF_TEST_CASE_BODY(mkdir__ok)
{
    fs::mkdir(fs::path("dir"), 0755);
    ATF_REQUIRE(lookup(".", "dir", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__enoent);
ATF_TEST_CASE_BODY(mkdir__enoent)
{
    try {
        fs::mkdir(fs::path("dir1/dir2"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
    ATF_REQUIRE(!lookup(".", "dir1", DT_DIR));
    ATF_REQUIRE(!lookup(".", "dir2", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__one_component);
ATF_TEST_CASE_BODY(mkdir_p__one_component)
{
    ATF_REQUIRE(!lookup(".", "new-dir", DT_DIR));
    fs::mkdir_p(fs::path("new-dir"), 0755);
    ATF_REQUIRE(lookup(".", "new-dir", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__many_components);
ATF_TEST_CASE_BODY(mkdir_p__many_components)
{
    ATF_REQUIRE(!lookup(".", "a", DT_DIR));
    fs::mkdir_p(fs::path("a/b/c"), 0755);
    ATF_REQUIRE(lookup(".", "a", DT_DIR));
    ATF_REQUIRE(lookup("a", "b", DT_DIR));
    ATF_REQUIRE(lookup("a/b", "c", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__already_exists);
ATF_TEST_CASE_BODY(mkdir_p__already_exists)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    fs::mkdir_p(fs::path("a/b"), 0755);
}


ATF_TEST_CASE(mkdir_p__eacces)
ATF_TEST_CASE_HEAD(mkdir_p__eacces)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(mkdir_p__eacces)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    ATF_REQUIRE(::chmod("a/b", 0555) != -1);
    try {
        fs::mkdir_p(fs::path("a/b/c/d"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(EACCES, e.original_errno());
    }
    ATF_REQUIRE(lookup(".", "a", DT_DIR));
    ATF_REQUIRE(lookup("a", "b", DT_DIR));
    ATF_REQUIRE(!lookup(".", "c", DT_DIR));
    ATF_REQUIRE(!lookup("a", "c", DT_DIR));
    ATF_REQUIRE(!lookup("a/b", "c", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdtemp)
ATF_TEST_CASE_BODY(mkdtemp)
{
    const fs::path tmpdir = fs::current_path() / "tmp";
    utils::setenv("TMPDIR", tmpdir.str());
    fs::mkdir(tmpdir, 0755);

    const std::string dir_template("tempdir.XXXXXX");
    const fs::path tempdir = fs::mkdtemp(dir_template);
    ATF_REQUIRE(!lookup("tmp", dir_template.c_str(), DT_DIR));
    ATF_REQUIRE(lookup("tmp", tempdir.leaf_name().c_str(), DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkstemp)
ATF_TEST_CASE_BODY(mkstemp)
{
    const fs::path tmpdir = fs::current_path() / "tmp";
    utils::setenv("TMPDIR", tmpdir.str());
    fs::mkdir(tmpdir, 0755);

    const std::string file_template("tempfile.XXXXXX");
    const fs::path tempfile = fs::mkstemp(file_template);
    ATF_REQUIRE(!lookup("tmp", file_template.c_str(), DT_REG));
    ATF_REQUIRE(lookup("tmp", tempfile.leaf_name().c_str(), DT_REG));
}


ATF_TEST_CASE_WITHOUT_HEAD(rm_r__empty);
ATF_TEST_CASE_BODY(rm_r__empty)
{
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::rm_r(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(rm_r__files_and_directories);
ATF_TEST_CASE_BODY(rm_r__files_and_directories)
{
    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file("root/.hidden_file", "");
    fs::mkdir(fs::path("root/.hidden_dir"), 0755);
    atf::utils::create_file("root/.hidden_dir/a", "");
    atf::utils::create_file("root/file", "");
    atf::utils::create_file("root/with spaces", "");
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir1/dir2"), 0755);
    atf::utils::create_file("root/dir1/dir2/file", "");
    fs::mkdir(fs::path("root/dir1/dir3"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::rm_r(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(rmdir__ok)
ATF_TEST_CASE_BODY(rmdir__ok)
{
    ATF_REQUIRE(::mkdir("foo", 0755) != -1);
    ATF_REQUIRE(::access("foo", X_OK) == 0);
    fs::rmdir(fs::path("foo"));
    ATF_REQUIRE(::access("foo", X_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(rmdir__fail)
ATF_TEST_CASE_BODY(rmdir__fail)
{
    ATF_REQUIRE_THROW_RE(fs::system_error, "Removal of foo failed",
                         fs::rmdir(fs::path("foo")));
}


ATF_TEST_CASE_WITHOUT_HEAD(unlink__ok)
ATF_TEST_CASE_BODY(unlink__ok)
{
    atf::utils::create_file("foo", "");
    ATF_REQUIRE(::access("foo", R_OK) == 0);
    fs::unlink(fs::path("foo"));
    ATF_REQUIRE(::access("foo", R_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(unlink__fail)
ATF_TEST_CASE_BODY(unlink__fail)
{
    ATF_REQUIRE_THROW_RE(fs::system_error, "Removal of foo failed",
                         fs::unlink(fs::path("foo")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, current_path__ok);
    ATF_ADD_TEST_CASE(tcs, current_path__enoent);

    ATF_ADD_TEST_CASE(tcs, exists);

    ATF_ADD_TEST_CASE(tcs, find_in_path__no_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__empty_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__one_component);
    ATF_ADD_TEST_CASE(tcs, find_in_path__many_components);
    ATF_ADD_TEST_CASE(tcs, find_in_path__current_directory);
    ATF_ADD_TEST_CASE(tcs, find_in_path__always_absolute);

    ATF_ADD_TEST_CASE(tcs, mkdir__ok);
    ATF_ADD_TEST_CASE(tcs, mkdir__enoent);

    ATF_ADD_TEST_CASE(tcs, mkdir_p__one_component);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__many_components);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__already_exists);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__eacces);

    ATF_ADD_TEST_CASE(tcs, mkdtemp);

    ATF_ADD_TEST_CASE(tcs, mkstemp);

    ATF_ADD_TEST_CASE(tcs, rm_r__empty);
    ATF_ADD_TEST_CASE(tcs, rm_r__files_and_directories);

    ATF_ADD_TEST_CASE(tcs, rmdir__ok);
    ATF_ADD_TEST_CASE(tcs, rmdir__fail);

    ATF_ADD_TEST_CASE(tcs, unlink__ok);
    ATF_ADD_TEST_CASE(tcs, unlink__fail);
}
