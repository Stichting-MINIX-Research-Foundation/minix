// Copyright 2011 Google Inc.
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

#include "engine/drivers/list_tests.hpp"

extern "C" {
#include <sys/stat.h>

#include <unistd.h>
}

#include <map>
#include <set>

#include <atf-c++.hpp>

#include "cli/cmd_list.hpp"
#include "cli/common.ipp"
#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace list_tests = engine::drivers::list_tests;
namespace fs = utils::fs;

using utils::none;
using utils::optional;


namespace {


/// Gets the path to the helpers for this test program.
///
/// \param test_case A pointer to the currently running test case.
///
/// \return The path to the helpers binary.
static fs::path
helpers(const atf::tests::tc* test_case)
{
    return fs::path(test_case->get_config_var("srcdir")) /
        "list_tests_helpers";
}


/// Hooks to capture the incremental listing of test cases.
class capture_hooks : public list_tests::base_hooks {
public:
    /// Set of the listed test cases in a program:test_case form.
    std::set< std::string > test_cases;

    /// Called when a test case is identified in a test suite.
    ///
    /// \param test_case The data describing the test case.
    virtual void
    got_test_case(const engine::test_case& test_case)
    {
        test_cases.insert(F("%s:%s") %
                          test_case.container_test_program().relative_path() %
                          test_case.name());
    }
};


/// Creates a mock test suite.
///
/// \param tc Pointer to the caller test case; needed to obtain the srcdir
///     variable of the caller.
/// \param source_root Basename of the directory that will contain the
///     Kyuafiles.
/// \param build_root Basename of the directory that will contain the test
///     programs.  May or may not be the same as source_root.
static void
create_helpers(const atf::tests::tc* tc, const fs::path& source_root,
               const fs::path& build_root)
{
    ATF_REQUIRE(::mkdir(source_root.c_str(), 0755) != -1);
    ATF_REQUIRE(::mkdir((source_root / "dir").c_str(), 0755) != -1);
    if (source_root != build_root) {
        ATF_REQUIRE(::mkdir(build_root.c_str(), 0755) != -1);
        ATF_REQUIRE(::mkdir((build_root / "dir").c_str(), 0755) != -1);
    }
    ATF_REQUIRE(::symlink(helpers(tc).c_str(),
                          (build_root / "dir/program").c_str()) != -1);

    atf::utils::create_file(
        (source_root / "Kyuafile").str(),
        "syntax(2)\n"
        "include('dir/Kyuafile')\n");

    atf::utils::create_file(
        (source_root / "dir/Kyuafile").str(),
        "syntax(2)\n"
        "atf_test_program{name='program', test_suite='suite-name'}\n");
}


/// Runs the mock test suite.
///
/// \param source_root Path to the directory that contains the Kyuafiles.
/// \param build_root If not none, path to the directory that contains the test
///     programs.
/// \param hooks The hooks to use during the listing.
/// \param filter_program If not null, the filter on the test program name.
/// \param filter_test_case If not null, the filter on the test case name.
///
/// \return The result data of the driver.
static list_tests::result
run_helpers(const fs::path& source_root,
            const optional< fs::path > build_root,
            list_tests::base_hooks& hooks,
            const char* filter_program = NULL,
            const char* filter_test_case = NULL)
{
    std::set< engine::test_filter > filters;
    if (filter_program != NULL && filter_test_case != NULL)
        filters.insert(engine::test_filter(fs::path(filter_program),
                                           filter_test_case));

    return list_tests::drive(source_root / "Kyuafile", build_root, filters,
                             hooks);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(one_test_case);
ATF_TEST_CASE_BODY(one_test_case)
{
    utils::setenv("TESTS", "some_properties");
    capture_hooks hooks;
    create_helpers(this, fs::path("root"), fs::path("root"));
    run_helpers(fs::path("root"), none, hooks);

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(many_test_cases);
ATF_TEST_CASE_BODY(many_test_cases)
{
    utils::setenv("TESTS", "no_properties some_properties");
    capture_hooks hooks;
    create_helpers(this, fs::path("root"), fs::path("root"));
    run_helpers(fs::path("root"), none, hooks);

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:no_properties");
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(filter_match);
ATF_TEST_CASE_BODY(filter_match)
{
    utils::setenv("TESTS", "no_properties some_properties");
    capture_hooks hooks;
    create_helpers(this, fs::path("root"), fs::path("root"));
    run_helpers(fs::path("root"), none, hooks, "dir/program",
                "some_properties");

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(build_root);
ATF_TEST_CASE_BODY(build_root)
{
    utils::setenv("TESTS", "no_properties some_properties");
    capture_hooks hooks;
    create_helpers(this, fs::path("source"), fs::path("build"));
    run_helpers(fs::path("source"), utils::make_optional(fs::path("build")),
                hooks);

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:no_properties");
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(crash);
ATF_TEST_CASE_BODY(crash)
{
    utils::setenv("TESTS", "crash_list some_properties");
    capture_hooks hooks;
    create_helpers(this, fs::path("root"), fs::path("root"));
    run_helpers(fs::path("root"), none, hooks, "dir/program");

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:__test_cases_list__");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, one_test_case);
    ATF_ADD_TEST_CASE(tcs, many_test_cases);
    ATF_ADD_TEST_CASE(tcs, filter_match);
    ATF_ADD_TEST_CASE(tcs, build_root);
    ATF_ADD_TEST_CASE(tcs, crash);
}
