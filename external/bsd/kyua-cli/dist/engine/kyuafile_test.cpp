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

#include "engine/kyuafile.hpp"

#include <stdexcept>
#include <typeinfo>

#include <atf-c++.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>
#include <lutok/test_utils.hpp>

#include "engine/exceptions.hpp"
#include "engine/test_program.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;

using utils::none;


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__empty);
ATF_TEST_CASE_BODY(kyuafile__load__empty)
{
    atf::utils::create_file("config", "syntax(2)\n");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(0, suite.test_programs().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__some_programs);
ATF_TEST_CASE_BODY(kyuafile__load__some_programs)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('one-suite')\n"
        "atf_test_program{name='1st'}\n"
        "atf_test_program{name='2nd', test_suite='first'}\n"
        "plain_test_program{name='3rd'}\n"
        "plain_test_program{name='4th', test_suite='second'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("dir"), 0755);
    atf::utils::create_file(
        "dir/config",
        "syntax(2)\n"
        "atf_test_program{name='1st', test_suite='other-suite'}\n"
        "include('subdir/config')\n");

    fs::mkdir(fs::path("dir/subdir"), 0755);
    atf::utils::create_file(
        "dir/subdir/config",
        "syntax(2)\n"
        "atf_test_program{name='5th', test_suite='last-suite'}\n");

    atf::utils::create_file("1st", "");
    atf::utils::create_file("2nd", "");
    atf::utils::create_file("3rd", "");
    atf::utils::create_file("4th", "");
    atf::utils::create_file("dir/1st", "");
    atf::utils::create_file("dir/subdir/5th", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(6, suite.test_programs().size());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[0]->interface_name());
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[0]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[1]->interface_name());
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[1]->test_suite_name());

    ATF_REQUIRE_EQ("plain", suite.test_programs()[2]->interface_name());
    ATF_REQUIRE_EQ(fs::path("3rd"), suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[2]->test_suite_name());

    ATF_REQUIRE_EQ("plain", suite.test_programs()[3]->interface_name());
    ATF_REQUIRE_EQ(fs::path("4th"), suite.test_programs()[3]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[3]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[4]->interface_name());
    ATF_REQUIRE_EQ(fs::path("dir/1st"),
                   suite.test_programs()[4]->relative_path());
    ATF_REQUIRE_EQ("other-suite", suite.test_programs()[4]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[5]->interface_name());
    ATF_REQUIRE_EQ(fs::path("dir/subdir/5th"),
                   suite.test_programs()[5]->relative_path());
    ATF_REQUIRE_EQ("last-suite", suite.test_programs()[5]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__metadata);
ATF_TEST_CASE_BODY(kyuafile__load__metadata)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='1st', test_suite='first',"
        " allowed_architectures='amd64 i386', timeout=15}\n"
        "plain_test_program{name='2nd', test_suite='second',"
        " required_files='foo /bar//baz', required_user='root'}\n");
    atf::utils::create_file("1st", "");
    atf::utils::create_file("2nd", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none);
    ATF_REQUIRE_EQ(2, suite.test_programs().size());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[0]->interface_name());
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());
    const engine::metadata md1 = engine::metadata_builder()
        .add_allowed_architecture("amd64")
        .add_allowed_architecture("i386")
        .set_timeout(datetime::delta(15, 0))
        .build();
    ATF_REQUIRE_EQ(md1, suite.test_programs()[0]->get_metadata());

    ATF_REQUIRE_EQ("plain", suite.test_programs()[1]->interface_name());
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[1]->test_suite_name());
    const engine::metadata md2 = engine::metadata_builder()
        .add_required_file(fs::path("foo"))
        .add_required_file(fs::path("/bar/baz"))
        .set_required_user("root")
        .build();
    ATF_REQUIRE_EQ(md2, suite.test_programs()[1]->get_metadata());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__current_directory);
ATF_TEST_CASE_BODY(kyuafile__load__current_directory)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='one', test_suite='first'}\n"
        "include('config2')\n");

    atf::utils::create_file(
        "config2",
        "syntax(2)\n"
        "test_suite('second')\n"
        "atf_test_program{name='two'}\n");

    atf::utils::create_file("one", "");
    atf::utils::create_file("two", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[1]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__other_directory);
ATF_TEST_CASE_BODY(kyuafile__load__other_directory)
{
    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file(
        "root/config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("root/dir"), 0755);
    atf::utils::create_file(
        "root/dir/config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "atf_test_program{name='two', test_suite='def'}\n"
        "atf_test_program{name='three'}\n");

    atf::utils::create_file("root/one", "");
    atf::utils::create_file("root/dir/two", "");
    atf::utils::create_file("root/dir/three", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("root/config"), none);
    ATF_REQUIRE_EQ(fs::path("root"), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("root"), suite.build_root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("def", suite.test_programs()[1]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/three"),
                   suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("foo", suite.test_programs()[2]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__build_directory);
ATF_TEST_CASE_BODY(kyuafile__load__build_directory)
{
    fs::mkdir(fs::path("srcdir"), 0755);
    atf::utils::create_file(
        "srcdir/config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("srcdir/dir"), 0755);
    atf::utils::create_file(
        "srcdir/dir/config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "atf_test_program{name='two', test_suite='def'}\n"
        "atf_test_program{name='three'}\n");

    fs::mkdir(fs::path("builddir"), 0755);
    atf::utils::create_file("builddir/one", "");
    fs::mkdir(fs::path("builddir/dir"), 0755);
    atf::utils::create_file("builddir/dir/two", "");
    atf::utils::create_file("builddir/dir/three", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("srcdir/config"), utils::make_optional(fs::path("builddir")));
    ATF_REQUIRE_EQ(fs::path("srcdir"), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("builddir"), suite.build_root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("builddir/one").to_absolute(),
                   suite.test_programs()[0]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("builddir/dir/two").to_absolute(),
                   suite.test_programs()[1]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("def", suite.test_programs()[1]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("builddir/dir/three").to_absolute(),
                   suite.test_programs()[2]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("dir/three"),
                   suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("foo", suite.test_programs()[2]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_program_not_basename);
ATF_TEST_CASE_BODY(kyuafile__load__test_program_not_basename)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "atf_test_program{name='./ls'}\n");

    atf::utils::create_file("one", "");
    ATF_REQUIRE_THROW_RE(engine::load_error, "./ls.*path components",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__lua_error);
ATF_TEST_CASE_BODY(kyuafile__load__lua_error)
{
    atf::utils::create_file("config", "this syntax is invalid\n");

    ATF_REQUIRE_THROW(engine::load_error, engine::kyuafile::load(
                          fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__not_called);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__not_called)
{
    atf::utils::create_file("config", "");

    ATF_REQUIRE_THROW_RE(engine::load_error, "syntax.* never called",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__deprecated_format);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__deprecated_format)
{
    atf::utils::create_file("config", "syntax('kyuafile', 1)\n");
    (void)engine::kyuafile::load(fs::path("config"), none);

    atf::utils::create_file("config", "syntax('foo', 1)\n");
    ATF_REQUIRE_THROW_RE(engine::load_error, "must be 'kyuafile'",
                         engine::kyuafile::load(fs::path("config"), none));

    atf::utils::create_file("config", "syntax('config', 2)\n");
    ATF_REQUIRE_THROW_RE(engine::load_error, "only takes one argument",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__twice);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__twice)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "syntax(2)\n");

    ATF_REQUIRE_THROW_RE(engine::load_error, "Can only call syntax.* once",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__bad_version);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__bad_version)
{
    atf::utils::create_file("config", "syntax(12)\n");

    ATF_REQUIRE_THROW_RE(engine::load_error, "Unsupported file version 12",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_suite__twice);
ATF_TEST_CASE_BODY(kyuafile__load__test_suite__twice)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "test_suite('bar')\n");

    ATF_REQUIRE_THROW_RE(engine::load_error, "Can only call test_suite.* once",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_file);
ATF_TEST_CASE_BODY(kyuafile__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(engine::load_error, "Load of 'missing' failed",
                         engine::kyuafile::load(fs::path("missing"), none));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_test_program);
ATF_TEST_CASE_BODY(kyuafile__load__missing_test_program)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='one', test_suite='first'}\n"
        "atf_test_program{name='two', test_suite='first'}\n");

    atf::utils::create_file("one", "");

    ATF_REQUIRE_THROW_RE(engine::load_error, "Non-existent.*'two'",
                         engine::kyuafile::load(fs::path("config"), none));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__empty);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__some_programs);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__metadata);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__current_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__other_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__build_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_program_not_basename);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__not_called);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__deprecated_format);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__twice);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__bad_version);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_suite__twice);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_file);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_test_program);
}
