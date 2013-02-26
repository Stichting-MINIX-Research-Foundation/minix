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

#include "engine/drivers/scan_action.hpp"

#include <set>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;

using utils::none;
using utils::optional;


namespace {


/// Records the callback values for futher investigation.
class capture_hooks : public scan_action::base_hooks {
public:
    /// The captured action ID, if any.
    optional< int64_t > _action_id;

    /// The captured action, if any.
    optional< engine::action > _action;

    /// The captured results, flattened as "program:test_case:result".
    std::set< std::string > _results;

    /// Callback executed when an action is found.
    ///
    /// \param action_id The identifier of the loaded action.
    /// \param action The action loaded from the database.
    void got_action(const int64_t action_id,
                    const engine::action& action)
    {
        PRE(!_action_id);
        _action_id = action_id;
        PRE(!_action);
        _action = action;
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void got_result(store::results_iterator& iter)
    {
        const char* type;
        switch (iter.result().type()) {
        case engine::test_result::passed: type = "passed"; break;
        case engine::test_result::skipped: type = "skipped"; break;
        default:
            UNREACHABLE_MSG("Formatting unimplemented");
        }
        _results.insert(F("%s:%s:%s:%s:%s:%s") %
                        iter.test_program()->absolute_path() %
                        iter.test_case_name() % type % iter.result().reason() %
                        iter.duration().seconds % iter.duration().useconds);
    }
};


/// Populates a test database with a new action.
///
/// It is OK to call this function multiple times on the same file.  Doing this
/// will generate a new action every time on the test database.
///
/// \param db_name The database to update.
/// \param count A number that indicates how many elements to insert in the
///     action.  Can be used to determine from the caller which particular
///     action has been loaded.
///
/// \return The identifier of the committed action.
static int64_t
populate_db(const char* db_name, const int count)
{
    store::backend backend = store::backend::open_rw(fs::path(db_name));

    store::transaction tx = backend.start();

    std::map< std::string, std::string > env;
    for (int i = 0; i < count; i++)
        env[F("VAR%s") % i] = F("Value %s") % i;
    const engine::context context(fs::path("/root"), env);
    const engine::action action(context);
    const int64_t context_id = tx.put_context(context);
    const int64_t action_id = tx.put_action(action, context_id);

    for (int i = 0; i < count; i++) {
        const engine::test_program test_program(
            "plain", fs::path(F("dir/prog_%s") % i), fs::path("/root"),
            F("suite_%s") % i, engine::metadata_builder().build());
        const int64_t tp_id = tx.put_test_program(test_program, action_id);

        for (int j = 0; j < count; j++) {
            const engine::test_case test_case(
                "plain", test_program, "main",
                engine::metadata_builder().build());
            const engine::test_result result(engine::test_result::skipped,
                                             F("Count %s") % j);
            const int64_t tc_id = tx.put_test_case(test_case, tp_id);
            const datetime::timestamp start =
                datetime::timestamp::from_microseconds(1000010);
            const datetime::timestamp end =
                datetime::timestamp::from_microseconds(5000020 + i + j);
            tx.put_result(result, tc_id, start, end);
        }
    }

    tx.commit();

    return action_id;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(latest_action);
ATF_TEST_CASE_BODY(latest_action)
{
    (void)populate_db("test.db", 3);
    const int64_t action_id = populate_db("test.db", 2);

    capture_hooks hooks;
    scan_action::drive(fs::path("test.db"), none, hooks);

    ATF_REQUIRE_EQ(action_id, hooks._action_id.get());

    std::map< std::string, std::string > env;
    env["VAR0"] = "Value 0";
    env["VAR1"] = "Value 1";
    const engine::context context(fs::path("/root"), env);
    const engine::action action(context);
    ATF_REQUIRE(action == hooks._action.get());

    std::set< std::string > results;
    results.insert("/root/dir/prog_0:main:skipped:Count 0:4:10");
    results.insert("/root/dir/prog_0:main:skipped:Count 1:4:11");
    results.insert("/root/dir/prog_1:main:skipped:Count 0:4:11");
    results.insert("/root/dir/prog_1:main:skipped:Count 1:4:12");
    ATF_REQUIRE(results == hooks._results);
}


ATF_TEST_CASE_WITHOUT_HEAD(explicit_action);
ATF_TEST_CASE_BODY(explicit_action)
{
    (void)populate_db("test.db", 5);
    const int64_t action_id = populate_db("test.db", 1);
    (void)populate_db("test.db", 2);

    capture_hooks hooks;
    scan_action::drive(fs::path("test.db"),
                       optional< int64_t >(action_id), hooks);

    ATF_REQUIRE_EQ(action_id, hooks._action_id.get());

    std::map< std::string, std::string > env;
    env["VAR0"] = "Value 0";
    const engine::context context(fs::path("/root"), env);
    const engine::action action(context);
    ATF_REQUIRE(action == hooks._action.get());

    std::set< std::string > results;
    results.insert("/root/dir/prog_0:main:skipped:Count 0:4:10");
    ATF_REQUIRE(results == hooks._results);
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_db);
ATF_TEST_CASE_BODY(missing_db)
{
    capture_hooks hooks;
    ATF_REQUIRE_THROW(store::error,
                      scan_action::drive(fs::path("test.db"), none, hooks));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, latest_action);
    ATF_ADD_TEST_CASE(tcs, explicit_action);
    ATF_ADD_TEST_CASE(tcs, missing_db);
}
