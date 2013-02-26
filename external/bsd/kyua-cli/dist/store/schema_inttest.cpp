// Copyright 2013 Google Inc.
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

#include "store/backend.hpp"

#include <fstream>
#include <map>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/transaction.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/stream.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;
namespace units = utils::units;


/// Executes an SQL script within a database.
///
/// \param db Database in which to run the script.
/// \param path Path to the data file.
static void
exec_db_file(sqlite::database& db, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        ATF_FAIL(F("Failed to open %s") % path);
    db.exec(utils::read_stream(input));
}


/// Validates the contents of the action with identifier 1.
///
/// \param transaction An open read transaction in the backend.
static void
check_action_1(store::transaction& transaction)
{
    const fs::path root("/some/root");
    std::map< std::string, std::string > environment;
    const engine::context context_1(root, environment);

    const engine::action action_1(context_1);

    ATF_REQUIRE_EQ(action_1, transaction.get_action(1));

    store::results_iterator iter = transaction.get_action_results(1);
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 2.
///
/// \param transaction An open read transaction in the backend.
static void
check_action_2(store::transaction& transaction)
{
    const fs::path root("/test/suite/root");
    std::map< std::string, std::string > environment;
    environment["HOME"] = "/home/test";
    environment["PATH"] = "/bin:/usr/bin";
    const engine::context context_2(root, environment);

    const engine::action action_2(context_2);

    ATF_REQUIRE_EQ(action_2, transaction.get_action(2));

    engine::test_program test_program_1(
        "plain", fs::path("foo_test"), fs::path("/test/suite/root"),
        "suite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_1(new engine::test_case(
            "plain", test_program_1, "main",
            engine::metadata_builder().build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_1);
        test_program_1.set_test_cases(test_cases);
    }
    const engine::test_result result_1(engine::test_result::passed);

    engine::test_program test_program_2(
        "plain", fs::path("subdir/another_test"), fs::path("/test/suite/root"),
        "subsuite-name", engine::metadata_builder()
        .set_timeout(datetime::delta(10, 0)).build());
    {
        const engine::test_case_ptr test_case_2(new engine::test_case(
            "plain", test_program_2, "main", engine::metadata_builder()
            .set_timeout(datetime::delta(10, 0)).build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_2);
        test_program_2.set_test_cases(test_cases);
    }
    const engine::test_result result_2(engine::test_result::failed,
                                       "Exited with code 1");

    engine::test_program test_program_3(
        "plain", fs::path("subdir/bar_test"), fs::path("/test/suite/root"),
        "subsuite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_3(new engine::test_case(
            "plain", test_program_3, "main",
            engine::metadata_builder().build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_3);
        test_program_3.set_test_cases(test_cases);
    }
    const engine::test_result result_3(engine::test_result::broken,
                                       "Received signal 1");

    engine::test_program test_program_4(
        "plain", fs::path("top_test"), fs::path("/test/suite/root"),
        "suite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_4(new engine::test_case(
            "plain", test_program_4, "main",
            engine::metadata_builder().build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_4);
        test_program_4.set_test_cases(test_cases);
    }
    const engine::test_result result_4(engine::test_result::expected_failure,
                                       "Known bug");

    engine::test_program test_program_5(
        "plain", fs::path("last_test"), fs::path("/test/suite/root"),
        "suite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_5(new engine::test_case(
            "plain", test_program_5, "main",
            engine::metadata_builder().build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_5);
        test_program_5.set_test_cases(test_cases);
    }
    const engine::test_result result_5(engine::test_result::skipped,
                                       "Does not apply");

    store::results_iterator iter = transaction.get_action_results(2);
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_1, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_1, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(10, 500), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_5, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_5, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(6, 0), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_2, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_2, iter.result());
    ATF_REQUIRE_EQ("Test stdout", iter.stdout_contents());
    ATF_REQUIRE_EQ("Test stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(datetime::delta(0, 898821), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_3, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_3, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(7, 481932), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_4, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_4, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(0, 20000), iter.duration());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 3.
///
/// \param transaction An open read transaction in the backend.
static void
check_action_3(store::transaction& transaction)
{
    const fs::path root("/usr/tests");
    std::map< std::string, std::string > environment;
    environment["PATH"] = "/bin:/usr/bin";
    const engine::context context_3(root, environment);

    const engine::action action_3(context_3);

    ATF_REQUIRE_EQ(action_3, transaction.get_action(3));

    engine::test_program test_program_6(
        "atf", fs::path("complex_test"), fs::path("/usr/tests"),
        "suite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_6(new engine::test_case(
            "atf", test_program_6, "this_passes",
            engine::metadata_builder().build()));
        const engine::test_case_ptr test_case_7(new engine::test_case(
            "atf", test_program_6, "this_fails",
            engine::metadata_builder()
            .set_description("Test description")
            .set_has_cleanup(true)
            .set_required_memory(units::bytes(128))
            .set_required_user("root").build()));
        const engine::test_case_ptr test_case_8(new engine::test_case(
            "atf", test_program_6, "this_skips",
            engine::metadata_builder()
            .add_allowed_architecture("powerpc")
            .add_allowed_architecture("x86_64")
            .add_allowed_platform("amd64")
            .add_allowed_platform("macppc")
            .add_required_config("X-foo")
            .add_required_config("unprivileged_user")
            .add_required_file(fs::path("/the/data/file"))
            .add_required_program(fs::path("/bin/ls"))
            .add_required_program(fs::path("cp"))
            .set_description("Test explanation")
            .set_has_cleanup(true)
            .set_required_memory(units::bytes(512))
            .set_required_user("unprivileged")
            .set_timeout(datetime::delta(600, 0))
            .build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_6);
        test_cases.push_back(test_case_7);
        test_cases.push_back(test_case_8);
        test_program_6.set_test_cases(test_cases);
    }
    const engine::test_result result_6(engine::test_result::passed);
    const engine::test_result result_7(engine::test_result::failed,
                                       "Some reason");
    const engine::test_result result_8(engine::test_result::skipped,
                                       "Another reason");

    engine::test_program test_program_7(
        "atf", fs::path("simple_test"), fs::path("/usr/tests"),
        "subsuite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_9(new engine::test_case(
            "atf", test_program_7, "main",
            engine::metadata_builder()
            .set_description("More text")
            .set_has_cleanup(true)
            .set_required_memory(units::bytes(128))
            .set_required_user("unprivileged")
            .build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_9);
        test_program_7.set_test_cases(test_cases);
    }
    const engine::test_result result_9(engine::test_result::failed,
                                       "Exited with code 1");

    store::results_iterator iter = transaction.get_action_results(3);
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_fails", iter.test_case_name());
    ATF_REQUIRE_EQ(result_7, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(1, 897182), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_passes", iter.test_case_name());
    ATF_REQUIRE_EQ(result_6, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(6, 0), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_skips", iter.test_case_name());
    ATF_REQUIRE_EQ(result_8, iter.result());
    ATF_REQUIRE_EQ("Another stdout", iter.stdout_contents());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(0, 817987), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_7, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_9, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE_EQ("Another stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(datetime::delta(9, 961700), iter.duration());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 4.
///
/// \param transaction An open read transaction in the backend.
static void
check_action_4(store::transaction& transaction)
{
    const fs::path root("/usr/tests");
    std::map< std::string, std::string > environment;
    environment["LANG"] = "C";
    environment["PATH"] = "/bin:/usr/bin";
    environment["TERM"] = "xterm";
    const engine::context context_4(root, environment);

    const engine::action action_4(context_4);

    ATF_REQUIRE_EQ(action_4, transaction.get_action(4));

    engine::test_program test_program_8(
        "plain", fs::path("subdir/another_test"), fs::path("/usr/tests"),
        "subsuite-name", engine::metadata_builder()
        .set_timeout(datetime::delta(10, 0)).build());
    {
        const engine::test_case_ptr test_case_10(new engine::test_case(
            "plain", test_program_8, "main",
            engine::metadata_builder()
            .set_timeout(datetime::delta(10, 0)).build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_10);
        test_program_8.set_test_cases(test_cases);
    }
    const engine::test_result result_10(engine::test_result::failed,
                                       "Exit failure");

    engine::test_program test_program_9(
        "atf", fs::path("complex_test"), fs::path("/usr/tests"),
        "suite-name", engine::metadata_builder().build());
    {
        const engine::test_case_ptr test_case_11(new engine::test_case(
            "atf", test_program_9, "this_passes",
            engine::metadata_builder().build()));
        const engine::test_case_ptr test_case_12(new engine::test_case(
            "atf", test_program_9, "this_fails",
            engine::metadata_builder()
            .set_description("Test description")
            .set_required_user("root")
            .build()));
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case_11);
        test_cases.push_back(test_case_12);
        test_program_9.set_test_cases(test_cases);
    }
    const engine::test_result result_11(engine::test_result::passed);
    const engine::test_result result_12(engine::test_result::failed,
                                        "Some reason");

    store::results_iterator iter = transaction.get_action_results(4);
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_9, *iter.test_program());
    ATF_REQUIRE_EQ("this_fails", iter.test_case_name());
    ATF_REQUIRE_EQ(result_12, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(1, 905000), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_9, *iter.test_program());
    ATF_REQUIRE_EQ("this_passes", iter.test_case_name());
    ATF_REQUIRE_EQ(result_11, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(datetime::delta(0, 500000), iter.duration());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_8, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_10, iter.result());
    ATF_REQUIRE_EQ("Test stdout", iter.stdout_contents());
    ATF_REQUIRE_EQ("Test stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(datetime::delta(1, 0), iter.duration());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// Validates the contents of an open database agains good known values.
///
/// \param transaction An open read-only backend.
static void
check_data(store::backend& backend)
{
    store::transaction transaction = backend.start();
    check_action_1(transaction);
    check_action_2(transaction);
    check_action_3(transaction);
    check_action_4(transaction);
}


ATF_TEST_CASE(current_schema);
ATF_TEST_CASE_HEAD(current_schema)
{
    logging::set_inmemory();
    const std::string required_files =
        store::detail::schema_file().str()
        + " " + (fs::path(get_config_var("srcdir")) / "testdata_v2.sql").str();
    set_md_var("require.files", required_files);
}
ATF_TEST_CASE_BODY(current_schema)
{
    const fs::path testpath("test.db");

    sqlite::database db = sqlite::database::open(
        testpath, sqlite::open_readwrite | sqlite::open_create);
    exec_db_file(db, store::detail::schema_file());
    exec_db_file(db, fs::path(get_config_var("srcdir")) / "testdata_v2.sql");
    db.close();

    store::backend backend = store::backend::open_ro(testpath);
    check_data(backend);
}


ATF_TEST_CASE(migrate_schema__v1_to_v2);
ATF_TEST_CASE_HEAD(migrate_schema__v1_to_v2)
{
    logging::set_inmemory();
    const std::string required_files =
        store::detail::migration_file(1, 2).str()
        + " " + (fs::path(get_config_var("srcdir")) / "schema_v1.sql").str()
        + " " + (fs::path(get_config_var("srcdir")) / "testdata_v1.sql").str();
    set_md_var("require.files", required_files);
}
ATF_TEST_CASE_BODY(migrate_schema__v1_to_v2)
{
    const fs::path testpath("test.db");

    sqlite::database db = sqlite::database::open(
        testpath, sqlite::open_readwrite | sqlite::open_create);
    exec_db_file(db, fs::path(get_config_var("srcdir")) / "schema_v1.sql");
    exec_db_file(db, fs::path(get_config_var("srcdir")) / "testdata_v1.sql");
    db.close();

    store::migrate_schema(fs::path("test.db"));

    store::backend backend = store::backend::open_ro(testpath);
    check_data(backend);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, current_schema);

    ATF_ADD_TEST_CASE(tcs, migrate_schema__v1_to_v2);
}
