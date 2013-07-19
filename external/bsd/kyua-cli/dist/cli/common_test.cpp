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

#include "cli/common.hpp"

#include <fstream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;

using utils::optional;


namespace {


/// Syntactic sugar to instantiate engine::test_filter objects.
inline engine::test_filter
mkfilter(const char* test_program, const char* test_case)
{
    return engine::test_filter(fs::path(test_program), test_case);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(build_root_path__default);
ATF_TEST_CASE_BODY(build_root_path__default)
{
    std::map< std::string, std::vector< std::string > > options;
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE(!cli::build_root_path(mock_cmdline));
}


ATF_TEST_CASE_WITHOUT_HEAD(build_root_path__explicit);
ATF_TEST_CASE_BODY(build_root_path__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["build-root"].push_back("/my//path");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE(cli::build_root_path(mock_cmdline));
    ATF_REQUIRE_EQ("/my/path", cli::build_root_path(mock_cmdline).get().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__ok);
ATF_TEST_CASE_BODY(get_home__ok)
{
    const fs::path home("/foo/bar");
    utils::setenv("HOME", home.str());
    const optional< fs::path > computed = cli::get_home();
    ATF_REQUIRE(computed);
    ATF_REQUIRE_EQ(home, computed.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__missing);
ATF_TEST_CASE_BODY(get_home__missing)
{
    utils::unsetenv("HOME");
    ATF_REQUIRE(!cli::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__invalid);
ATF_TEST_CASE_BODY(get_home__invalid)
{
    utils::setenv("HOME", "");
    ATF_REQUIRE(!cli::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile_path__default);
ATF_TEST_CASE_BODY(kyuafile_path__default)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back(cli::kyuafile_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_EQ(cli::kyuafile_option.default_value(),
                   cli::kyuafile_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile_path__explicit);
ATF_TEST_CASE_BODY(kyuafile_path__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back("/my//path");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_EQ("/my/path", cli::kyuafile_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__default__create_directory__ok);
ATF_TEST_CASE_BODY(store_path__default__create_directory__ok)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    utils::setenv("HOME", home.str());

    ATF_REQUIRE(!fs::exists(home / ".kyua"));
    ATF_REQUIRE_EQ(home / ".kyua/store.db", cli::store_path(mock_cmdline));
    ATF_REQUIRE(fs::exists(home / ".kyua"));
}


ATF_TEST_CASE(store_path__default__create_directory__fail);
ATF_TEST_CASE_HEAD(store_path__default__create_directory__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(store_path__default__create_directory__fail)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    utils::setenv("HOME", home.str());
    fs::mkdir(home, 0555);

    ATF_REQUIRE_THROW(fs::error, cli::store_path(mock_cmdline).str());
    ATF_REQUIRE(!fs::exists(home / ".kyua"));
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__default__no_home);
ATF_TEST_CASE_BODY(store_path__default__no_home)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    utils::unsetenv("HOME");

    ATF_REQUIRE_EQ("kyua-store.db", cli::store_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__explicit);
ATF_TEST_CASE_BODY(store_path__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back("/my//path");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    ATF_REQUIRE_EQ("/my/path", cli::store_path(mock_cmdline).str());
    ATF_REQUIRE(!fs::exists(home / ".kyua"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__none);
ATF_TEST_CASE_BODY(parse_filters__none)
{
    const cmdline::args_vector args;
    const std::set< engine::test_filter > filters = cli::parse_filters(args);
    ATF_REQUIRE(filters.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__ok);
ATF_TEST_CASE_BODY(parse_filters__ok)
{
    cmdline::args_vector args;
    args.push_back("foo");
    args.push_back("bar/baz");
    args.push_back("other:abc");
    args.push_back("other:bcd");
    const std::set< engine::test_filter > filters = cli::parse_filters(args);

    std::set< engine::test_filter > exp_filters;
    exp_filters.insert(mkfilter("foo", ""));
    exp_filters.insert(mkfilter("bar/baz", ""));
    exp_filters.insert(mkfilter("other", "abc"));
    exp_filters.insert(mkfilter("other", "bcd"));

    ATF_REQUIRE(exp_filters == filters);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__duplicate);
ATF_TEST_CASE_BODY(parse_filters__duplicate)
{
    cmdline::args_vector args;
    args.push_back("foo/bar//baz");
    args.push_back("hello/world:yes");
    args.push_back("foo//bar/baz");
    ATF_REQUIRE_THROW_RE(cmdline::error, "Duplicate.*'foo/bar/baz'",
                         cli::parse_filters(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__nondisjoint);
ATF_TEST_CASE_BODY(parse_filters__nondisjoint)
{
    cmdline::args_vector args;
    args.push_back("foo/bar");
    args.push_back("hello/world:yes");
    args.push_back("foo/bar:baz");
    ATF_REQUIRE_THROW_RE(cmdline::error, "'foo/bar'.*'foo/bar:baz'.*disjoint",
                         cli::parse_filters(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(report_unused_filters__none);
ATF_TEST_CASE_BODY(report_unused_filters__none)
{
    std::set< engine::test_filter > unused;

    cmdline::ui_mock ui;
    ATF_REQUIRE(!cli::report_unused_filters(unused, &ui));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(report_unused_filters__some);
ATF_TEST_CASE_BODY(report_unused_filters__some)
{
    std::set< engine::test_filter > unused;
    unused.insert(mkfilter("a/b", ""));
    unused.insert(mkfilter("hey/d", "yes"));

    cmdline::ui_mock ui;
    cmdline::init("progname");
    ATF_REQUIRE(cli::report_unused_filters(unused, &ui));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(2, ui.err_log().size());
    ATF_REQUIRE( atf::utils::grep_collection("No.*matched.*'a/b'",
                                             ui.err_log()));
    ATF_REQUIRE( atf::utils::grep_collection("No.*matched.*'hey/d:yes'",
                                             ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_delta);
ATF_TEST_CASE_BODY(format_delta)
{
    ATF_REQUIRE_EQ("0.000s", cli::format_delta(datetime::delta()));
    ATF_REQUIRE_EQ("0.012s", cli::format_delta(datetime::delta(0, 12300)));
    ATF_REQUIRE_EQ("0.999s", cli::format_delta(datetime::delta(0, 999000)));
    ATF_REQUIRE_EQ("51.321s", cli::format_delta(datetime::delta(51, 321000)));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_result__no_reason);
ATF_TEST_CASE_BODY(format_result__no_reason)
{
    ATF_REQUIRE_EQ("passed", cli::format_result(
        engine::test_result(engine::test_result::passed)));
    ATF_REQUIRE_EQ("failed", cli::format_result(
        engine::test_result(engine::test_result::failed)));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_result__with_reason);
ATF_TEST_CASE_BODY(format_result__with_reason)
{
    ATF_REQUIRE_EQ("broken: Something", cli::format_result(
        engine::test_result(engine::test_result::broken, "Something")));
    ATF_REQUIRE_EQ("expected_failure: A B C", cli::format_result(
        engine::test_result(engine::test_result::expected_failure, "A B C")));
    ATF_REQUIRE_EQ("failed: More text", cli::format_result(
        engine::test_result(engine::test_result::failed, "More text")));
    ATF_REQUIRE_EQ("skipped: Bye", cli::format_result(
        engine::test_result(engine::test_result::skipped, "Bye")));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_test_case_id__test_case);
ATF_TEST_CASE_BODY(format_test_case_id__test_case)
{
    const engine::test_program test_program(
        "mock", fs::path("foo/bar/baz"), fs::path("unused-root"),
        "unused-suite-name", engine::metadata_builder().build());
    const engine::test_case test_case("mock", test_program, "abc",
                                      engine::metadata_builder().build());
    ATF_REQUIRE_EQ("foo/bar/baz:abc", cli::format_test_case_id(test_case));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_test_case_id__test_filter);
ATF_TEST_CASE_BODY(format_test_case_id__test_filter)
{
    const engine::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ("foo/bar:baz", cli::format_test_case_id(filter));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, build_root_path__default);
    ATF_ADD_TEST_CASE(tcs, build_root_path__explicit);

    ATF_ADD_TEST_CASE(tcs, get_home__ok);
    ATF_ADD_TEST_CASE(tcs, get_home__missing);
    ATF_ADD_TEST_CASE(tcs, get_home__invalid);

    ATF_ADD_TEST_CASE(tcs, kyuafile_path__default);
    ATF_ADD_TEST_CASE(tcs, kyuafile_path__explicit);

    ATF_ADD_TEST_CASE(tcs, store_path__default__create_directory__ok);
    ATF_ADD_TEST_CASE(tcs, store_path__default__create_directory__fail);
    ATF_ADD_TEST_CASE(tcs, store_path__default__no_home);
    ATF_ADD_TEST_CASE(tcs, store_path__explicit);

    ATF_ADD_TEST_CASE(tcs, parse_filters__none);
    ATF_ADD_TEST_CASE(tcs, parse_filters__ok);
    ATF_ADD_TEST_CASE(tcs, parse_filters__duplicate);
    ATF_ADD_TEST_CASE(tcs, parse_filters__nondisjoint);

    ATF_ADD_TEST_CASE(tcs, report_unused_filters__none);
    ATF_ADD_TEST_CASE(tcs, report_unused_filters__some);

    ATF_ADD_TEST_CASE(tcs, format_delta);

    ATF_ADD_TEST_CASE(tcs, format_result__no_reason);
    ATF_ADD_TEST_CASE(tcs, format_result__with_reason);

    ATF_ADD_TEST_CASE(tcs, format_test_case_id__test_case);
    ATF_ADD_TEST_CASE(tcs, format_test_case_id__test_filter);
}
