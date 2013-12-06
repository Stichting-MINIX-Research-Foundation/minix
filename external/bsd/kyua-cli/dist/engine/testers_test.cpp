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

#include "engine/testers.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;

using utils::none;
using utils::optional;


namespace {


/// Creates a mock tester that exits cleanly.
///
/// The interface accepted by the tester is 'mock'.  This tester outputs the
/// arguments passed to it and then prints a message to both the stdout and the
/// stderr.
///
/// \param exit_status Code to exit with.
static void
create_mock_tester_exit(const int exit_status)
{
    atf::utils::create_file(
        "kyua-mock-tester",
        F("#! /bin/sh\n"
          "while [ ${#} -gt 0 ]; do\n"
          "    echo \"Arg: ${1}\"\n"
          "    shift\n"
          "done\n"
          "echo 'tester output'\n"
          "echo 'tester error' 1>&2\n"
          "exit %s\n") % exit_status);
    ATF_REQUIRE(::chmod("kyua-mock-tester", 0755) != -1);

    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
}


/// Creates a mock tester that receives a signal.
///
/// The interface accepted by the tester is 'mock'.  This tester outputs the
/// arguments passed to it and then prints a message to both the stdout and the
/// stderr.
///
/// \param term_sig Signal to deliver to the tester.  If the tester does not
///     exit due to this reason, it exits with an arbitrary non-zero code.
static void
create_mock_tester_signal(const int term_sig)
{
    atf::utils::create_file(
        "kyua-mock-tester",
        F("#! /bin/sh\n"
          "while [ ${#} -gt 0 ]; do\n"
          "    echo \"Arg: ${1}\"\n"
          "    shift\n"
          "done\n"
          "echo 'tester output'\n"
          "echo 'tester error' 1>&2\n"
          "kill -%s $$\n"
          "echo 'signal did not terminate the process\n"
          "exit 0\n") % term_sig);
    ATF_REQUIRE(::chmod("kyua-mock-tester", 0755) != -1);

    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(tester__list__defaults);
ATF_TEST_CASE_BODY(tester__list__defaults)
{
    create_mock_tester_exit(EXIT_SUCCESS);
    engine::tester tester("mock", none, none);
    const std::string output = tester.list(fs::path("/foo/bar"));

    const std::string exp_output =
        "Arg: list\n"
        "Arg: /foo/bar\n"
        "tester output\n"
        "tester error\n";
    ATF_REQUIRE_EQ(exp_output, output);
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__list__explicit_common_args);
ATF_TEST_CASE_BODY(tester__list__explicit_common_args)
{
    const passwd::user user("fake", 123, 456);
    const datetime::delta timeout(15, 0);

    create_mock_tester_exit(EXIT_SUCCESS);
    engine::tester tester("mock", utils::make_optional(user),
                          utils::make_optional(timeout));
    const std::string output = tester.list(fs::path("/another/program/1"));

    const std::string exp_output =
        "Arg: -u123\n"
        "Arg: -g456\n"
        "Arg: -t15\n"
        "Arg: list\n"
        "Arg: /another/program/1\n"
        "tester output\n"
        "tester error\n";
    ATF_REQUIRE_EQ(exp_output, output);
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__list__unknown_interface);
ATF_TEST_CASE_BODY(tester__list__unknown_interface)
{
    utils::setenv("KYUA_TESTERSDIR", ".");
    engine::tester tester("non-existent", none, none);
    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface non-existent",
                         tester.list(fs::path("does-not-matter")));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__list__tester_fails);
ATF_TEST_CASE_BODY(tester__list__tester_fails)
{
    create_mock_tester_exit(EXIT_FAILURE);
    engine::tester tester("mock", none, none);
    ATF_REQUIRE_THROW_RE(
        engine::error,
        "Tester did not exit cleanly:.*tester output.*tester error",
        tester.list(fs::path("does-not-matter")));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__list__tester_crashes);
ATF_TEST_CASE_BODY(tester__list__tester_crashes)
{
    create_mock_tester_signal(SIGKILL);
    engine::tester tester("mock", none, none);
    ATF_REQUIRE_THROW_RE(
        engine::error,
        "Tester did not exit cleanly:.*tester output.*tester error",
        tester.list(fs::path("does-not-matter")));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__test__defaults);
ATF_TEST_CASE_BODY(tester__test__defaults)
{
    std::map< std::string, std::string > vars;

    create_mock_tester_exit(EXIT_FAILURE);
    engine::tester tester("mock", none, none);
    tester.test(fs::path("/foo/bar"), "test-case", fs::path("/the/result/file"),
                fs::path("tester.out"), fs::path("tester.err"), vars);

    const std::string exp_output =
        "Arg: test\n"
        "Arg: /foo/bar\n"
        "Arg: test-case\n"
        "Arg: /the/result/file\n"
        "tester output\n";
    const std::string exp_error =
        "tester error\n";
    ATF_REQUIRE(atf::utils::compare_file("tester.out", exp_output));
    ATF_REQUIRE(atf::utils::compare_file("tester.err", exp_error));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__test__explicit_common_args_and_vars);
ATF_TEST_CASE_BODY(tester__test__explicit_common_args_and_vars)
{
    const passwd::user user("fake", 123, 456);
    const datetime::delta timeout(15, 0);

    std::map< std::string, std::string > vars;
    vars["var1"] = "value1";
    vars["variable-2"] = "value with spaces";

    create_mock_tester_exit(EXIT_SUCCESS);
    engine::tester tester("mock", utils::make_optional(user),
                          utils::make_optional(timeout));
    tester.test(fs::path("/foo/bar"), "test-case", fs::path("/the/result/file"),
                fs::path("tester.out"), fs::path("tester.err"), vars);

    const std::string exp_output =
        "Arg: -u123\n"
        "Arg: -g456\n"
        "Arg: -t15\n"
        "Arg: test\n"
        "Arg: -vvar1=value1\n"
        "Arg: -vvariable-2=value with spaces\n"
        "Arg: /foo/bar\n"
        "Arg: test-case\n"
        "Arg: /the/result/file\n"
        "tester output\n";
    const std::string exp_error =
        "tester error\n";
    ATF_REQUIRE(atf::utils::compare_file("tester.out", exp_output));
    ATF_REQUIRE(atf::utils::compare_file("tester.err", exp_error));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__test__unknown_interface);
ATF_TEST_CASE_BODY(tester__test__unknown_interface)
{
    const std::map< std::string, std::string > vars;

    utils::setenv("KYUA_TESTERSDIR", ".");
    engine::tester tester("non-existent", none, none);
    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface non-existent",
                         tester.test(fs::path("foo"), "bar", fs::path("baz"),
                                     fs::path("out"), fs::path("err"), vars));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__test__tester_fails);
ATF_TEST_CASE_BODY(tester__test__tester_fails)
{
    const std::map< std::string, std::string > vars;

    create_mock_tester_exit(2);
    engine::tester tester("mock", none, none);
    ATF_REQUIRE_THROW_RE(
        engine::error,
        "Tester failed with code 2; this is a bug",
        tester.test(fs::path("foo"), "bar", fs::path("baz"),
                    fs::path("out"), fs::path("err"), vars));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester__test__tester_crashes);
ATF_TEST_CASE_BODY(tester__test__tester_crashes)
{
    const std::map< std::string, std::string > vars;

    create_mock_tester_signal(SIGKILL);
    engine::tester tester("mock", none, none);
    ATF_REQUIRE_THROW_RE(
        engine::error,
        F("Tester received signal %s; this is a bug") % SIGKILL,
        tester.test(fs::path("foo"), "bar", fs::path("baz"),
                    fs::path("out"), fs::path("err"), vars));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__default);
ATF_TEST_CASE_BODY(tester_path__default)
{
    ATF_REQUIRE(atf::utils::file_exists(engine::tester_path("atf").str()));
    ATF_REQUIRE(atf::utils::file_exists(engine::tester_path("plain").str()));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__custom);
ATF_TEST_CASE_BODY(tester_path__custom)
{
    fs::mkdir(fs::path("testers"), 0755);
    atf::utils::create_file("testers/kyua-mock-1-tester", "Not a binary");
    atf::utils::create_file("testers/kyua-mock-2-tester", "Not a binary");
    utils::setenv("KYUA_TESTERSDIR", (fs::current_path() / "testers").str());

    const fs::path mock1 = engine::tester_path("mock-1");
    ATF_REQUIRE(mock1.is_absolute());
    ATF_REQUIRE(atf::utils::file_exists(mock1.str()));

    const fs::path mock2 = engine::tester_path("mock-2");
    ATF_REQUIRE(mock2.is_absolute());
    ATF_REQUIRE(atf::utils::file_exists(mock2.str()));

    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface mock-3",
                         engine::tester_path("mock-3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__cached);
ATF_TEST_CASE_BODY(tester_path__cached)
{
    fs::mkdir(fs::path("testers"), 0755);
    atf::utils::create_file("testers/kyua-mock-tester", "Not a binary");
    utils::setenv("KYUA_TESTERSDIR", (fs::current_path() / "testers").str());

    const fs::path mock = engine::tester_path("mock");
    ATF_REQUIRE(atf::utils::file_exists(mock.str()));
    ATF_REQUIRE(::unlink(mock.c_str()) != -1);
    ATF_REQUIRE(!atf::utils::file_exists(mock.str()));
    ATF_REQUIRE_EQ(mock, engine::tester_path("mock"));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__empty);
ATF_TEST_CASE_BODY(tester_path__empty)
{
    fs::mkdir(fs::path("testers"), 0755);
    atf::utils::create_file("testers/kyua--tester", "Not a binary");
    utils::setenv("KYUA_TESTERSDIR", (fs::current_path() / "testers").str());

    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface ",
                         engine::tester_path(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__missing);
ATF_TEST_CASE_BODY(tester_path__missing)
{
    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface plain",
                         engine::tester_path("plain"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, tester__list__defaults);
    ATF_ADD_TEST_CASE(tcs, tester__list__explicit_common_args);
    ATF_ADD_TEST_CASE(tcs, tester__list__unknown_interface);
    ATF_ADD_TEST_CASE(tcs, tester__list__tester_fails);
    ATF_ADD_TEST_CASE(tcs, tester__list__tester_crashes);

    ATF_ADD_TEST_CASE(tcs, tester__test__defaults);
    ATF_ADD_TEST_CASE(tcs, tester__test__explicit_common_args_and_vars);
    ATF_ADD_TEST_CASE(tcs, tester__test__unknown_interface);
    ATF_ADD_TEST_CASE(tcs, tester__test__tester_fails);
    ATF_ADD_TEST_CASE(tcs, tester__test__tester_crashes);

    ATF_ADD_TEST_CASE(tcs, tester_path__default);
    ATF_ADD_TEST_CASE(tcs, tester_path__custom);
    ATF_ADD_TEST_CASE(tcs, tester_path__cached);
    ATF_ADD_TEST_CASE(tcs, tester_path__empty);
    ATF_ADD_TEST_CASE(tcs, tester_path__missing);
}
