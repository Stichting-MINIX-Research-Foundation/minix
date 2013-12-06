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

#include "engine/test_program.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <sstream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/test_result.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


namespace {


/// Creates a mock tester that receives a signal.
///
/// \param term_sig Signal to deliver to the tester.  If the tester does not
///     exit due to this reason, it exits with an arbitrary non-zero code.
static void
create_mock_tester_signal(const int term_sig)
{
    const std::string tester_name = "kyua-mock-tester";

    atf::utils::create_file(
        tester_name,
        F("#! /bin/sh\n"
          "kill -%s $$\n"
          "exit 0\n") % term_sig);
    ATF_REQUIRE(::chmod(tester_name.c_str(), 0755) != -1);

    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const engine::metadata md = engine::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", md);
    ATF_REQUIRE_EQ("mock", test_program.interface_name());
    ATF_REQUIRE_EQ(fs::path("binary"), test_program.relative_path());
    ATF_REQUIRE_EQ(fs::current_path() / "root/binary",
                   test_program.absolute_path());
    ATF_REQUIRE_EQ(fs::path("root"), test_program.root());
    ATF_REQUIRE_EQ("suite-name", test_program.test_suite_name());
    ATF_REQUIRE_EQ(md, test_program.get_metadata());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__ok);
ATF_TEST_CASE_BODY(find__ok)
{
    const engine::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    const engine::test_case_ptr test_case = test_program.find("main");
    ATF_REQUIRE_EQ(fs::path("non-existent"),
                   test_case->container_test_program().relative_path());
    ATF_REQUIRE_EQ("main", test_case->name());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__missing);
ATF_TEST_CASE_BODY(find__missing)
{
    const engine::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    ATF_REQUIRE_THROW_RE(engine::not_found_error,
                         "case.*abc.*program.*non-existent",
                         test_program.find("abc"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__get);
ATF_TEST_CASE_BODY(test_cases__get)
{
    const engine::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    const engine::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());
    ATF_REQUIRE_EQ(fs::path("non-existent"),
                   test_cases[0]->container_test_program().relative_path());
    ATF_REQUIRE_EQ("main", test_cases[0]->name());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__some);
ATF_TEST_CASE_BODY(test_cases__some)
{
    engine::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());

    engine::test_cases_vector exp_test_cases;
    const engine::test_case test_case("plain", test_program, "main",
                                      engine::metadata_builder().build());
    exp_test_cases.push_back(engine::test_case_ptr(
        new engine::test_case(test_case)));
    test_program.set_test_cases(exp_test_cases);

    ATF_REQUIRE_EQ(exp_test_cases, test_program.test_cases());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__tester_fails);
ATF_TEST_CASE_BODY(test_cases__tester_fails)
{
    engine::test_program test_program(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    create_mock_tester_signal(SIGSEGV);

    const engine::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());

    const engine::test_case_ptr& test_case = test_cases[0];
    ATF_REQUIRE_EQ("__test_cases_list__", test_case->name());

    ATF_REQUIRE(test_case->fake_result());
    const engine::test_result result = test_case->fake_result().get();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Tester did not exit cleanly", result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__copy)
{
    const engine::test_program tp1(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    const engine::test_program tp2 = tp1;
    ATF_REQUIRE(  tp1 == tp2);
    ATF_REQUIRE(!(tp1 != tp2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__not_copy)
{
    const std::string base_interface("plain");
    const fs::path base_relative_path("the/test/program");
    const fs::path base_root("/the/root");
    const std::string base_test_suite("suite-name");
    const engine::metadata base_metadata = engine::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();

    engine::test_program base_tp(
        base_interface, base_relative_path, base_root, base_test_suite,
        base_metadata);

    engine::test_cases_vector base_tcs;
    {
        const engine::test_case tc1("plain", base_tp, "main",
                                    engine::metadata_builder().build());
        base_tcs.push_back(engine::test_case_ptr(new engine::test_case(tc1)));
    }
    base_tp.set_test_cases(base_tcs);

    // Construct with all same values.
    {
        engine::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata);

        engine::test_cases_vector other_tcs;
        {
            const engine::test_case tc1("plain", other_tp, "main",
                                        engine::metadata_builder().build());
            other_tcs.push_back(engine::test_case_ptr(
                new engine::test_case(tc1)));
        }
        other_tp.set_test_cases(other_tcs);

        ATF_REQUIRE(  base_tp == other_tp);
        ATF_REQUIRE(!(base_tp != other_tp));
    }

    // Different interface.
    {
        engine::test_program other_tp(
            "atf", base_relative_path, base_root, base_test_suite,
            base_metadata);
        other_tp.set_test_cases(base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different relative path.
    {
        engine::test_program other_tp(
            base_interface, fs::path("a/b/c"), base_root, base_test_suite,
            base_metadata);
        other_tp.set_test_cases(base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different root.
    {
        engine::test_program other_tp(
            base_interface, base_relative_path, fs::path("."), base_test_suite,
            base_metadata);
        other_tp.set_test_cases(base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test suite.
    {
        engine::test_program other_tp(
            base_interface, base_relative_path, base_root, "different-suite",
            base_metadata);
        other_tp.set_test_cases(base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different metadata.
    {
        engine::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            engine::metadata_builder().build());
        other_tp.set_test_cases(base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test cases.
    {
        engine::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata);

        engine::test_cases_vector other_tcs;
        {
            const engine::test_case tc1("atf", base_tp, "foo",
                                        engine::metadata_builder().build());
            other_tcs.push_back(engine::test_case_ptr(
                                    new engine::test_case(tc1)));
        }
        other_tp.set_test_cases(other_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(output__no_test_cases);
ATF_TEST_CASE_BODY(output__no_test_cases)
{
    engine::test_program tp(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name",
        engine::metadata_builder().add_allowed_architecture("a").build());
    tp.set_test_cases(engine::test_cases_vector());

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=[]}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_test_cases);
ATF_TEST_CASE_BODY(output__some_test_cases)
{
    engine::test_program tp(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name",
        engine::metadata_builder().add_allowed_architecture("a").build());

    const engine::test_case_ptr tc1(new engine::test_case(
        "plain", tp, "the-name", engine::metadata_builder()
        .add_allowed_platform("foo").add_custom("X-bar", "baz").build()));
    const engine::test_case_ptr tc2(new engine::test_case(
        "plain", tp, "another-name", engine::metadata_builder().build()));
    engine::test_cases_vector tcs;
    tcs.push_back(tc1);
    tcs.push_back(tc2);
    tp.set_test_cases(tcs);

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=["
        "test_case{interface='plain', name='the-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='foo', "
        "custom.X-bar='baz', description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}, "
        "test_case{interface='plain', name='another-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}]}",
        str.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    // TODO(jmmv): These tests have ceased to be realistic with the move to
    // TestersDesign.  We probably should have some (few!) integration tests for
    // the various known testers... or, alternatively, provide a mock tester to
    // run our tests with.
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, find__ok);
    ATF_ADD_TEST_CASE(tcs, find__missing);
    ATF_ADD_TEST_CASE(tcs, test_cases__get);
    ATF_ADD_TEST_CASE(tcs, test_cases__some);
    ATF_ADD_TEST_CASE(tcs, test_cases__tester_fails);

    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__not_copy);

    ATF_ADD_TEST_CASE(tcs, output__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, output__some_test_cases);
}
