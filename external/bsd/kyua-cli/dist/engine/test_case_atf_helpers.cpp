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

extern "C" {
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


namespace {


/// Creates an empty file in the given directory.
///
/// \param test_case The test case currently running.
/// \param directory The name of the configuration variable that holds the path
///     to the directory in which to create the cookie file.
/// \param name The name of the cookie file to create.
static void
create_cookie(const atf::tests::tc* test_case, const char* directory,
              const char* name)
{
    if (!test_case->has_config_var(directory))
        test_case->fail(std::string(name) + " not provided");

    const fs::path control_dir(test_case->get_config_var(directory));
    std::ofstream file((control_dir / name).c_str());
    if (!file)
        test_case->fail("Failed to create the control cookie");
    file.close();
}


}  // anonymous namespace


ATF_TEST_CASE_WITH_CLEANUP(check_cleanup_workdir);
ATF_TEST_CASE_HEAD(check_cleanup_workdir)
{
    set_md_var("require.config", "control_dir");
}
ATF_TEST_CASE_BODY(check_cleanup_workdir)
{
    std::ofstream cookie("workdir_cookie");
    cookie << "1234\n";
    cookie.close();
    skip("cookie created");
}
ATF_TEST_CASE_CLEANUP(check_cleanup_workdir)
{
    const fs::path control_dir(get_config_var("control_dir"));

    std::ifstream cookie("workdir_cookie");
    if (!cookie) {
        std::ofstream((control_dir / "missing_cookie").c_str()).close();
        std::exit(EXIT_FAILURE);
    }

    std::string value;
    cookie >> value;
    if (value != "1234") {
        std::ofstream((control_dir / "invalid_cookie").c_str()).close();
        std::exit(EXIT_FAILURE);
    }

    std::ofstream((control_dir / "cookie_ok").c_str()).close();
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(check_unprivileged);
ATF_TEST_CASE_BODY(check_unprivileged)
{
    if (::getuid() == 0)
        fail("Running as root, but I shouldn't be");

    std::ofstream file("cookie");
    if (!file)
        fail("Failed to create the cookie; work directory probably owned by "
             "root");
    file.close();
}


ATF_TEST_CASE_WITHOUT_HEAD(crash);
ATF_TEST_CASE_BODY(crash)
{
    std::abort();
}


ATF_TEST_CASE_WITH_CLEANUP(crash_cleanup);
ATF_TEST_CASE_HEAD(crash_cleanup)
{
}
ATF_TEST_CASE_BODY(crash_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(crash_cleanup)
{
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(create_cookie_in_control_dir);
ATF_TEST_CASE_BODY(create_cookie_in_control_dir)
{
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITHOUT_HEAD(create_cookie_in_workdir);
ATF_TEST_CASE_BODY(create_cookie_in_workdir)
{
    std::ofstream file("cookie");
    if (!file)
        fail("Failed to create the cookie");
    file.close();
}


ATF_TEST_CASE_WITH_CLEANUP(create_cookie_from_cleanup);
ATF_TEST_CASE_HEAD(create_cookie_from_cleanup)
{
}
ATF_TEST_CASE_BODY(create_cookie_from_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(create_cookie_from_cleanup)
{
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITH_CLEANUP(output);
ATF_TEST_CASE_HEAD(output)
{
}
ATF_TEST_CASE_BODY(output)
{
    std::cout << "Body message to stdout\n";
    std::cerr << "Body message to stderr\n";
}
ATF_TEST_CASE_CLEANUP(output)
{
    std::cout << "Cleanup message to stdout\n";
    std::cerr << "Cleanup message to stderr\n";
}


ATF_TEST_CASE_WITHOUT_HEAD(pass);
ATF_TEST_CASE_BODY(pass)
{
}


ATF_TEST_CASE(spawn_blocking_child);
ATF_TEST_CASE_HEAD(spawn_blocking_child)
{
    set_md_var("require.config", "control_dir");
}
ATF_TEST_CASE_BODY(spawn_blocking_child)
{
    pid_t pid = ::fork();
    if (pid == -1)
        fail("Cannot fork subprocess");
    else if (pid == 0) {
        for (;;)
            ::pause();
    } else {
        const fs::path name = fs::path(get_config_var("control_dir")) / "pid";
        std::ofstream pidfile(name.c_str());
        ATF_REQUIRE(pidfile);
        pidfile << pid;
        pidfile.close();
    }
}


ATF_TEST_CASE(timeout_body);
ATF_TEST_CASE_HEAD(timeout_body)
{
    if (has_config_var("timeout"))
        set_md_var("timeout", get_config_var("timeout"));
}
ATF_TEST_CASE_BODY(timeout_body)
{
    ::sleep(10);
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITH_CLEANUP(timeout_cleanup);
ATF_TEST_CASE_HEAD(timeout_cleanup)
{
    if (has_config_var("timeout"))
        set_md_var("timeout", get_config_var("timeout"));
}
ATF_TEST_CASE_BODY(timeout_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(timeout_cleanup)
{
    ::sleep(10);
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITHOUT_HEAD(validate_isolation);
ATF_TEST_CASE_BODY(validate_isolation)
{
    ATF_REQUIRE(utils::getenv("HOME").get() != "fake-value");
    ATF_REQUIRE(!utils::getenv("LANG"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, check_cleanup_workdir);
    ATF_ADD_TEST_CASE(tcs, check_unprivileged);
    ATF_ADD_TEST_CASE(tcs, crash);
    ATF_ADD_TEST_CASE(tcs, crash_cleanup);
    ATF_ADD_TEST_CASE(tcs, create_cookie_in_control_dir);
    ATF_ADD_TEST_CASE(tcs, create_cookie_in_workdir);
    ATF_ADD_TEST_CASE(tcs, create_cookie_from_cleanup);
    ATF_ADD_TEST_CASE(tcs, output);
    ATF_ADD_TEST_CASE(tcs, pass);
    ATF_ADD_TEST_CASE(tcs, spawn_blocking_child);
    ATF_ADD_TEST_CASE(tcs, timeout_body);
    ATF_ADD_TEST_CASE(tcs, timeout_cleanup);
    ATF_ADD_TEST_CASE(tcs, validate_isolation);
}
