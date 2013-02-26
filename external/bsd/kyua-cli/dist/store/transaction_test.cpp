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

#include "store/transaction.hpp"

#include <cstring>
#include <map>
#include <set>
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;
namespace units = utils::units;

using utils::none;
using utils::optional;


namespace {


/// Performs a test for a working put_result
///
/// \param result The result object to put.
/// \param result_type The textual name of the result to expect in the
///     database.
/// \param exp_reason The reason to expect in the database.  This is separate
///     from the result parameter so that we can handle passed() here as well.
///     Just provide NULL in this case.
static void
do_put_result_ok_test(const engine::test_result& result,
                      const char* result_type, const char* exp_reason)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const datetime::timestamp start_time = datetime::timestamp::from_values(
        2012, 01, 30, 22, 10, 00, 0);
    const datetime::timestamp end_time = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 30, 123456);
    tx.put_result(result, 312, start_time, end_time);
    tx.commit();

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT test_case_id, result_type, result_reason "
        "FROM test_results");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(312, stmt.column_int64(0));
    ATF_REQUIRE_EQ(result_type, stmt.column_text(1));
    if (exp_reason != NULL)
        ATF_REQUIRE_EQ(exp_reason, stmt.column_text(2));
    else
        ATF_REQUIRE(stmt.column_type(2) == sqlite::type_null);
    ATF_REQUIRE(!stmt.step());
}


}  // anonymous namespace


ATF_TEST_CASE(commit__ok);
ATF_TEST_CASE_HEAD(commit__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(commit__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    backend.database().exec("CREATE TABLE a (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a");
    tx.commit();
    backend.database().exec("SELECT * FROM a");
}


ATF_TEST_CASE(commit__fail);
ATF_TEST_CASE_HEAD(commit__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(commit__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    {
        store::transaction tx = backend.start();
        tx.put_context(context);
        backend.database().exec(
            "CREATE TABLE foo ("
            "a REFERENCES contexts(context_id) DEFERRABLE INITIALLY DEFERRED)");
        backend.database().exec("INSERT INTO foo VALUES (912378472)");
        ATF_REQUIRE_THROW(store::error, tx.commit());
    }
    // If the code attempts to maintain any state regarding the already-put
    // objects and the commit does not clean up correctly, this would fail in
    // some manner.
    store::transaction tx = backend.start();
    tx.put_context(context);
    tx.commit();
}


ATF_TEST_CASE(rollback__ok);
ATF_TEST_CASE_HEAD(rollback__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(rollback__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    backend.database().exec("CREATE TABLE a_table (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a_table");
    tx.rollback();
    ATF_REQUIRE_THROW_RE(sqlite::error, "a_table",
                         backend.database().exec("SELECT * FROM a_table"));
}


ATF_TEST_CASE(get_put_action__ok);
ATF_TEST_CASE_HEAD(get_put_action__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_action__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action exp_action1(context1);
    const engine::action exp_action2(context2);
    const engine::action exp_action3(context1);

    int64_t id1, id2, id3;
    {
        store::transaction tx = backend.start();
        const int64_t context1_id = tx.put_context(context1);
        const int64_t context2_id = tx.put_context(context2);
        id1 = tx.put_action(exp_action1, context1_id);
        id3 = tx.put_action(exp_action3, context1_id);
        id2 = tx.put_action(exp_action2, context2_id);
        tx.commit();
    }
    {
        store::transaction tx = backend.start();
        const engine::action action1 = tx.get_action(id1);
        const engine::action action2 = tx.get_action(id2);
        const engine::action action3 = tx.get_action(id3);
        tx.rollback();

        ATF_REQUIRE(exp_action1 == action1);
        ATF_REQUIRE(exp_action2 == action2);
        ATF_REQUIRE(exp_action3 == action3);
    }
}


ATF_TEST_CASE(get_put_action__get_fail__missing);
ATF_TEST_CASE_HEAD(get_put_action__get_fail__missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_action__get_fail__missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "action 523: does not exist",
                         tx.get_action(523));
}


ATF_TEST_CASE(get_put_action__get_fail__invalid_context);
ATF_TEST_CASE_HEAD(get_put_action__get_fail__invalid_context)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_action__get_fail__invalid_context)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    backend.database().exec("INSERT INTO actions (action_id, context_id) "
                            "VALUES (123, 456)");

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_action(123));
}


ATF_TEST_CASE(get_put_action__put_fail);
ATF_TEST_CASE_HEAD(get_put_action__put_fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_action__put_fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    const int64_t context_id = tx.put_context(context);
    const engine::action action(context);
    backend.database().exec("DROP TABLE actions");
    ATF_REQUIRE_THROW(store::error, tx.put_action(action, context_id));
    tx.commit();
}


ATF_TEST_CASE(get_action_results__none);
ATF_TEST_CASE_HEAD(get_action_results__none)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action_results__none)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    store::results_iterator iter = tx.get_action_results(1);
    ATF_REQUIRE(!iter);
}


ATF_TEST_CASE(get_action_results__many);
ATF_TEST_CASE_HEAD(get_action_results__many)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action_results__many)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    store::transaction tx = backend.start();

    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    const engine::action action(context);
    const int64_t context_id = tx.put_context(context);
    const int64_t action_id = tx.put_action(action, context_id);
    const int64_t action2_id = tx.put_action(action, context_id);

    const datetime::timestamp start_time1 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 10, 00, 0);
    const datetime::timestamp end_time1 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 30, 1234);
    const datetime::timestamp start_time2 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 40, 987);
    const datetime::timestamp end_time2 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 16, 0, 0);

    atf::utils::create_file("unused.txt", "unused file\n");

    engine::test_program test_program_1(
        "plain", fs::path("a/prog1"), fs::path("/the/root"), "suite1",
        engine::metadata_builder().build());
    engine::test_case_ptr test_case_1(new engine::test_case(
        "plain", test_program_1, "main", engine::metadata_builder().build()));
    engine::test_cases_vector test_cases_1;
    test_cases_1.push_back(test_case_1);
    test_program_1.set_test_cases(test_cases_1);
    const engine::test_result result_1(engine::test_result::passed);
    {
        const int64_t tp_id = tx.put_test_program(test_program_1, action_id);
        const int64_t tc_id = tx.put_test_case(*test_case_1, tp_id);
        atf::utils::create_file("prog1.out", "stdout of prog1\n");
        tx.put_test_case_file("__STDOUT__", fs::path("prog1.out"), tc_id);
        tx.put_test_case_file("unused.txt", fs::path("unused.txt"), tc_id);
        tx.put_result(result_1, tc_id, start_time1, end_time1);

        const int64_t tp2_id = tx.put_test_program(test_program_1, action2_id);
        const int64_t tc2_id = tx.put_test_case(*test_case_1, tp2_id);
        tx.put_test_case_file("__STDOUT__", fs::path("unused.txt"), tc2_id);
        tx.put_test_case_file("__STDERR__", fs::path("unused.txt"), tc2_id);
        tx.put_result(result_1, tc2_id, start_time1, end_time1);
    }

    engine::test_program test_program_2(
        "plain", fs::path("b/prog2"), fs::path("/the/root"), "suite2",
        engine::metadata_builder().build());
    engine::test_case_ptr test_case_2(new engine::test_case(
        "plain", test_program_2, "main", engine::metadata_builder().build()));
    engine::test_cases_vector test_cases_2;
    test_cases_2.push_back(test_case_2);
    test_program_2.set_test_cases(test_cases_2);
    const engine::test_result result_2(engine::test_result::failed,
                                       "Some text");
    {
        const int64_t tp_id = tx.put_test_program(test_program_2, action_id);
        const int64_t tc_id = tx.put_test_case(*test_case_2, tp_id);
        atf::utils::create_file("prog2.err", "stderr of prog2\n");
        tx.put_test_case_file("__STDERR__", fs::path("prog2.err"), tc_id);
        tx.put_test_case_file("unused.txt", fs::path("unused.txt"), tc_id);
        tx.put_result(result_2, tc_id, start_time2, end_time2);
    }

    tx.commit();

    store::transaction tx2 = backend.start();
    store::results_iterator iter = tx2.get_action_results(action_id);
    ATF_REQUIRE(iter);
    ATF_REQUIRE(test_program_1 == *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ("stdout of prog1\n", iter.stdout_contents());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE(result_1 == iter.result());
    ATF_REQUIRE(end_time1 - start_time1 == iter.duration());
    ATF_REQUIRE(++iter);
    ATF_REQUIRE(test_program_2 == *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE_EQ("stderr of prog2\n", iter.stderr_contents());
    ATF_REQUIRE(result_2 == iter.result());
    ATF_REQUIRE(end_time2 - start_time2 == iter.duration());
    ATF_REQUIRE(!++iter);
}


ATF_TEST_CASE(get_latest_action__ok);
ATF_TEST_CASE_HEAD(get_latest_action__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action exp_action1(context1);
    const engine::action exp_action2(context2);

    int64_t id2;
    {
        store::transaction tx = backend.start();
        const int64_t context1_id = tx.put_context(context1);
        const int64_t context2_id = tx.put_context(context2);
        (void)tx.put_action(exp_action1, context1_id);
        id2 = tx.put_action(exp_action2, context2_id);
        tx.commit();
    }
    {
        store::transaction tx = backend.start();
        const std::pair< int64_t, engine::action > latest_action =
            tx.get_latest_action();
        tx.rollback();

        ATF_REQUIRE(id2 == latest_action.first);
        ATF_REQUIRE(exp_action2 == latest_action.second);
    }
}


ATF_TEST_CASE(get_latest_action__none);
ATF_TEST_CASE_HEAD(get_latest_action__none)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__none)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "No actions", tx.get_latest_action());
}


ATF_TEST_CASE(get_latest_action__invalid_context);
ATF_TEST_CASE_HEAD(get_latest_action__invalid_context)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__invalid_context)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    backend.database().exec("INSERT INTO actions (action_id, context_id) "
                            "VALUES (123, 456)");

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_latest_action());
}


ATF_TEST_CASE(get_put_context__ok);
ATF_TEST_CASE_HEAD(get_put_context__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    std::map< std::string, std::string > env1;
    env1["A1"] = "foo";
    env1["A2"] = "bar";
    std::map< std::string, std::string > env2;
    const engine::context exp_context1(fs::path("/foo/bar"), env1);
    const engine::context exp_context2(fs::path("/foo/bar"), env1);
    const engine::context exp_context3(fs::path("/foo/baz"), env2);

    int64_t id1, id2, id3;
    {
        store::transaction tx = backend.start();
        id1 = tx.put_context(exp_context1);
        id3 = tx.put_context(exp_context3);
        id2 = tx.put_context(exp_context2);
        tx.commit();
    }
    {
        store::transaction tx = backend.start();
        const engine::context context1 = tx.get_context(id1);
        const engine::context context2 = tx.get_context(id2);
        const engine::context context3 = tx.get_context(id3);
        tx.rollback();

        ATF_REQUIRE(exp_context1 == context1);
        ATF_REQUIRE(exp_context2 == context2);
        ATF_REQUIRE(exp_context3 == context3);
    }
}


ATF_TEST_CASE(get_put_context__get_fail__missing);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_context(456));
}


ATF_TEST_CASE(get_put_context__get_fail__invalid_cwd);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__invalid_cwd)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__invalid_cwd)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    sqlite::statement stmt = backend.database().create_statement(
        "INSERT INTO contexts (context_id, cwd) VALUES (78, :cwd)");
    const char buffer[10] = "foo bar";
    stmt.bind(":cwd", sqlite::blob(buffer, sizeof(buffer)));
    stmt.step_without_results();

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 78: .*cwd.*not a string",
                         tx.get_context(78));
}


ATF_TEST_CASE(get_put_context__get_fail__invalid_env_vars);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__invalid_env_vars)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__invalid_env_vars)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (10, '/foo/bar')");
    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (20, '/foo/bar')");

    const char buffer[10] = "foo bar";

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (10, :var_name, 'abc')");
        stmt.bind(":var_name", sqlite::blob(buffer, sizeof(buffer)));
        stmt.step_without_results();
    }

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (20, 'abc', :var_value)");
        stmt.bind(":var_value", sqlite::blob(buffer, sizeof(buffer)));
        stmt.step_without_results();
    }

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 10: .*var_name.*not a string",
                         tx.get_context(10));
    ATF_REQUIRE_THROW_RE(store::error, "context 20: .*var_value.*not a string",
                         tx.get_context(20));
}


ATF_TEST_CASE(get_put_context__put_fail);
ATF_TEST_CASE_HEAD(get_put_context__put_fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__put_fail)
{
    (void)store::backend::open_rw(fs::path("test.db"));
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    ATF_REQUIRE_THROW(store::error, tx.put_context(context));
    tx.commit();
}


ATF_TEST_CASE(put_test_program__ok);
ATF_TEST_CASE_HEAD(put_test_program__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_program__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_custom("var1", "value1")
        .add_custom("var2", "value2")
        .build();
    const engine::test_program test_program(
        "mock", fs::path("the/binary"), fs::path("/some//root"),
        "the-suite", md);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const int64_t test_program_id = tx.put_test_program(test_program, 15);
    tx.commit();

    {
        sqlite::statement stmt = backend.database().create_statement(
            "SELECT * FROM test_programs");

        ATF_REQUIRE(stmt.step());
        ATF_REQUIRE_EQ(test_program_id,
                       stmt.safe_column_int64("test_program_id"));
        ATF_REQUIRE_EQ(15, stmt.safe_column_int64("action_id"));
        ATF_REQUIRE_EQ("/some/root/the/binary",
                       stmt.safe_column_text("absolute_path"));
        ATF_REQUIRE_EQ("/some/root", stmt.safe_column_text("root"));
        ATF_REQUIRE_EQ("the/binary", stmt.safe_column_text("relative_path"));
        ATF_REQUIRE_EQ("the-suite", stmt.safe_column_text("test_suite_name"));
        ATF_REQUIRE(!stmt.step());
    }
}


ATF_TEST_CASE(put_test_program__fail);
ATF_TEST_CASE_HEAD(put_test_program__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_program__fail)
{
    const engine::test_program test_program(
        "mock", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error, tx.put_test_program(test_program, -1));
    tx.commit();
}


ATF_TEST_CASE(put_test_case__ok);
ATF_TEST_CASE_HEAD(put_test_case__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case__ok)
{
    engine::test_program test_program(
        "atf", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());

    const engine::test_case_ptr test_case1(new engine::test_case(
        "atf", test_program, "tc1", engine::metadata_builder().build()));

    const engine::metadata md2 = engine::metadata_builder()
        .add_allowed_architecture("powerpc")
        .add_allowed_architecture("x86_64")
        .add_allowed_platform("amd64")
        .add_allowed_platform("macppc")
        .add_custom("X-user1", "value1")
        .add_custom("X-user2", "value2")
        .add_required_config("var1")
        .add_required_config("var2")
        .add_required_config("var3")
        .add_required_file(fs::path("/file1/yes"))
        .add_required_file(fs::path("/file2/foo"))
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("cp"))
        .set_description("The description")
        .set_has_cleanup(true)
        .set_required_memory(units::bytes::parse("1k"))
        .set_required_user("root")
        .set_timeout(datetime::delta(520, 0))
        .build();
    const engine::test_case_ptr test_case2(new engine::test_case(
        "atf", test_program, "tc2", md2));

    {
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case1);
        test_cases.push_back(test_case2);
        test_program.set_test_cases(test_cases);
    }

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    int64_t test_program_id;
    {
        store::transaction tx = backend.start();
        test_program_id = tx.put_test_program(test_program, 15);
        tx.put_test_case(*test_case1, test_program_id);
        tx.put_test_case(*test_case2, test_program_id);
        tx.commit();
    }

    store::transaction tx = backend.start();
    const engine::test_program_ptr loaded_test_program =
        store::detail::get_test_program(backend, test_program_id);
    ATF_REQUIRE(test_program == *loaded_test_program);
}


ATF_TEST_CASE(put_test_case__fail);
ATF_TEST_CASE_HEAD(put_test_case__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case__fail)
{
    // TODO(jmmv): Use a mock test program and test case.
    const engine::test_program test_program(
        "plain", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());
    const engine::test_case test_case("plain", test_program, "main",
                                      engine::metadata_builder().build());

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error, tx.put_test_case(test_case, -1));
    tx.commit();
}


ATF_TEST_CASE(put_test_case_file__empty);
ATF_TEST_CASE_HEAD(put_test_case_file__empty)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__empty)
{
    atf::utils::create_file("input.txt", "");

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const optional< int64_t > file_id = tx.put_test_case_file(
        "my-file", fs::path("input.txt"), 123L);
    tx.commit();
    ATF_REQUIRE(!file_id);

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_test_case_file__some);
ATF_TEST_CASE_HEAD(put_test_case_file__some)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__some)
{
    const char contents[] = "This is a test!";

    atf::utils::create_file("input.txt", contents);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const optional< int64_t > file_id = tx.put_test_case_file(
        "my-file", fs::path("input.txt"), 123L);
    tx.commit();
    ATF_REQUIRE(file_id);

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(123L, stmt.safe_column_int64("test_case_id"));
    ATF_REQUIRE_EQ("my-file", stmt.safe_column_text("file_name"));
    const sqlite::blob blob = stmt.safe_column_blob("contents");
    ATF_REQUIRE(std::strlen(contents) == static_cast< std::size_t >(blob.size));
    ATF_REQUIRE(std::memcmp(contents, blob.memory, blob.size) == 0);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_test_case_file__fail);
ATF_TEST_CASE_HEAD(put_test_case_file__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error,
                      tx.put_test_case_file("foo", fs::path("missing"), 1L));
    tx.commit();

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_result__ok__broken);
ATF_TEST_CASE_HEAD(put_result__ok__broken)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__broken)
{
    const engine::test_result result(engine::test_result::broken, "a b cd");
    do_put_result_ok_test(result, "broken", "a b cd");
}


ATF_TEST_CASE(put_result__ok__expected_failure);
ATF_TEST_CASE_HEAD(put_result__ok__expected_failure)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__expected_failure)
{
    const engine::test_result result(engine::test_result::expected_failure,
                                     "a b cd");
    do_put_result_ok_test(result, "expected_failure", "a b cd");
}


ATF_TEST_CASE(put_result__ok__failed);
ATF_TEST_CASE_HEAD(put_result__ok__failed)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__failed)
{
    const engine::test_result result(engine::test_result::failed, "a b cd");
    do_put_result_ok_test(result, "failed", "a b cd");
}


ATF_TEST_CASE(put_result__ok__passed);
ATF_TEST_CASE_HEAD(put_result__ok__passed)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__passed)
{
    const engine::test_result result(engine::test_result::passed);
    do_put_result_ok_test(result, "passed", NULL);
}


ATF_TEST_CASE(put_result__ok__skipped);
ATF_TEST_CASE_HEAD(put_result__ok__skipped)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__skipped)
{
    const engine::test_result result(engine::test_result::skipped, "a b cd");
    do_put_result_ok_test(result, "skipped", "a b cd");
}


ATF_TEST_CASE(put_result__fail);
ATF_TEST_CASE_HEAD(put_result__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__fail)
{
    const engine::test_result result(engine::test_result::broken, "foo");

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    const datetime::timestamp zero = datetime::timestamp::from_microseconds(0);
    ATF_REQUIRE_THROW(store::error, tx.put_result(result, -1, zero, zero));
    tx.commit();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, commit__ok);
    ATF_ADD_TEST_CASE(tcs, commit__fail);
    ATF_ADD_TEST_CASE(tcs, rollback__ok);

    ATF_ADD_TEST_CASE(tcs, get_put_action__ok);
    ATF_ADD_TEST_CASE(tcs, get_put_action__get_fail__missing);
    ATF_ADD_TEST_CASE(tcs, get_put_action__get_fail__invalid_context);
    ATF_ADD_TEST_CASE(tcs, get_put_action__put_fail);

    ATF_ADD_TEST_CASE(tcs, get_action_results__none);
    ATF_ADD_TEST_CASE(tcs, get_action_results__many);

    ATF_ADD_TEST_CASE(tcs, get_latest_action__ok);
    ATF_ADD_TEST_CASE(tcs, get_latest_action__none);
    ATF_ADD_TEST_CASE(tcs, get_latest_action__invalid_context);

    ATF_ADD_TEST_CASE(tcs, get_put_context__ok);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__missing);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__invalid_cwd);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__invalid_env_vars);
    ATF_ADD_TEST_CASE(tcs, get_put_context__put_fail);

    ATF_ADD_TEST_CASE(tcs, put_test_program__ok);
    ATF_ADD_TEST_CASE(tcs, put_test_program__fail);
    ATF_ADD_TEST_CASE(tcs, put_test_case__ok);
    ATF_ADD_TEST_CASE(tcs, put_test_case__fail);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__empty);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__some);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__fail);

    ATF_ADD_TEST_CASE(tcs, put_result__ok__broken);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__expected_failure);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__failed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__passed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__skipped);
    ATF_ADD_TEST_CASE(tcs, put_result__fail);
}
