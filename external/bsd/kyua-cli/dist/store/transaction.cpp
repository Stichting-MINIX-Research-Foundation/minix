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

extern "C" {
#include <stdint.h>
}

#include <fstream>
#include <map>
#include <utility>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/dbtypes.hpp"
#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/sqlite/transaction.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;
namespace units = utils::units;

using utils::none;
using utils::optional;


namespace {


/// Retrieves the environment variables of a context.
///
/// \param db The SQLite database.
/// \param context_id The identifier of the context.
///
/// \return The environment variables of the specified context.
///
/// \throw sqlite::error If there is a problem storing the variables.
static std::map< std::string, std::string >
get_env_vars(sqlite::database& db, const int64_t context_id)
{
    std::map< std::string, std::string > env;

    sqlite::statement stmt = db.create_statement(
        "SELECT var_name, var_value FROM env_vars "
        "WHERE context_id == :context_id");
    stmt.bind(":context_id", context_id);

    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("var_name");
        const std::string value = stmt.safe_column_text("var_value");
        env[name] = value;
    }

    return env;
}


/// Retrieves a metadata object.
///
/// \param db The SQLite database.
/// \param metadata_id The identifier of the metadata.
///
/// \return A new metadata object.
static engine::metadata
get_metadata(sqlite::database& db, const int64_t metadata_id)
{
    engine::metadata_builder builder;

    sqlite::statement stmt = db.create_statement(
        "SELECT * FROM metadatas WHERE metadata_id == :metadata_id");
    stmt.bind(":metadata_id", metadata_id);
    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("property_name");
        const std::string value = stmt.safe_column_text("property_value");
        builder.set_string(name, value);
    }

    return builder.build();
}


/// Gets a file from the database.
///
/// \param db The database to query the file from.
/// \param file_id The identifier of the file to be queried.
///
/// \return A textual representation of the file contents.
///
/// \throw integrity_error If there is any problem in the loaded data or if the
///     file cannot be found.
static std::string
get_file(sqlite::database& db, const int64_t file_id)
{
    sqlite::statement stmt = db.create_statement(
        "SELECT contents FROM files WHERE file_id == :file_id");
    stmt.bind(":file_id", file_id);
    if (!stmt.step())
        throw store::integrity_error(F("Cannot find referenced file %s") %
                                     file_id);

    try {
        const sqlite::blob raw_contents = stmt.safe_column_blob("contents");
        const std::string contents(
            static_cast< const char *>(raw_contents.memory), raw_contents.size);

        const bool more = stmt.step();
        INV(!more);

        return contents;
    } catch (const sqlite::error& e) {
        throw store::integrity_error(e.what());
    }
}


/// Gets all the test cases within a particular test program.
///
/// \param db The database to query the information from.
/// \param test_program_id The identifier of the test program whose test cases
///     to query.
/// \param test_program The test program itself, needed to establish a binding
///     between the loaded test cases and the test program.
/// \param interface The interface type of the test cases to be loaded.  This
///     assumes that all test cases within a test program share the same
///     interface, which is a pretty reasonable assumption.
///
/// \return The collection of loaded test cases.
///
/// \throw integrity_error If there is any problem in the loaded data.
static engine::test_cases_vector
get_test_cases(sqlite::database& db, const int64_t test_program_id,
               const engine::test_program& test_program,
               const std::string& interface)
{
    engine::test_cases_vector test_cases;

    sqlite::statement stmt = db.create_statement(
        "SELECT name, metadata_id "
        "FROM test_cases WHERE test_program_id == :test_program_id");
    stmt.bind(":test_program_id", test_program_id);
    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("name");
        const int64_t metadata_id = stmt.safe_column_int64("metadata_id");

        const engine::metadata metadata = get_metadata(db, metadata_id);
        engine::test_case_ptr test_case(
            new engine::test_case(interface, test_program, name, metadata));
        LD(F("Loaded test case '%s'") % test_case->name());
        test_cases.push_back(test_case);
    }

    return test_cases;
}


/// Retrieves a result from the database.
///
/// \param stmt The statement with the data for the result to load.
/// \param type_column The name of the column containing the type of the result.
/// \param reason_column The name of the column containing the reason for the
///     result, if any.
///
/// \return The loaded result.
///
/// \throw integrity_error If the data in the database is invalid.
static engine::test_result
parse_result(sqlite::statement& stmt, const char* type_column,
             const char* reason_column)
{
    using engine::test_result;

    try {
        const std::string type = stmt.safe_column_text(type_column);
        if (type == "passed") {
            if (stmt.column_type(stmt.column_id(reason_column)) !=
                sqlite::type_null)
                throw store::integrity_error("Result of type 'passed' has a "
                                             "non-NULL reason");
            return test_result(test_result::passed);
        } else if (type == "broken") {
            return test_result(test_result::broken,
                               stmt.safe_column_text(reason_column));
        } else if (type == "expected_failure") {
            return test_result(test_result::expected_failure,
                               stmt.safe_column_text(reason_column));
        } else if (type == "failed") {
            return test_result(test_result::failed,
                               stmt.safe_column_text(reason_column));
        } else if (type == "skipped") {
            return test_result(test_result::skipped,
                               stmt.safe_column_text(reason_column));
        } else {
            throw store::integrity_error(F("Unknown test result type %s") %
                                         type);
        }
    } catch (const sqlite::error& e) {
        throw store::integrity_error(e.what());
    }
}


/// Stores the environment variables of a context.
///
/// \param db The SQLite database.
/// \param context_id The identifier of the context.
/// \param env The environment variables to store.
///
/// \throw sqlite::error If there is a problem storing the variables.
static void
put_env_vars(sqlite::database& db, const int64_t context_id,
             const std::map< std::string, std::string >& env)
{
    sqlite::statement stmt = db.create_statement(
        "INSERT INTO env_vars (context_id, var_name, var_value) "
        "VALUES (:context_id, :var_name, :var_value)");
    stmt.bind(":context_id", context_id);
    for (std::map< std::string, std::string >::const_iterator iter =
             env.begin(); iter != env.end(); iter++) {
        stmt.bind(":var_name", (*iter).first);
        stmt.bind(":var_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }
}


/// Calculates the last rowid of a table.
///
/// \param db The SQLite database.
/// \param table Name of the table.
///
/// \return The last rowid; 0 if the table is empty.
static int64_t
last_rowid(sqlite::database& db, const std::string& table)
{
    sqlite::statement stmt = db.create_statement(
        F("SELECT MAX(ROWID) AS max_rowid FROM %s") % table);
    stmt.step();
    if (stmt.column_type(0) == sqlite::type_null) {
        return 0;
    } else {
        INV(stmt.column_type(0) == sqlite::type_integer);
        return stmt.column_int64(0);
    }
}


/// Stores a metadata object.
///
/// \param db The database into which to store the information.
/// \param md The metadata to store.
///
/// \return The identifier of the new metadata object.
static int64_t
put_metadata(sqlite::database& db, const engine::metadata& md)
{
    const engine::properties_map props = md.to_properties();

    const int64_t metadata_id = last_rowid(db, "metadatas");

    sqlite::statement stmt = db.create_statement(
        "INSERT INTO metadatas (metadata_id, property_name, property_value) "
        "VALUES (:metadata_id, :property_name, :property_value)");
    stmt.bind(":metadata_id", metadata_id);

    for (engine::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        stmt.bind(":property_name", (*iter).first);
        stmt.bind(":property_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }

    return metadata_id;
}


/// Stores an arbitrary file into the database as a BLOB.
///
/// \param db The database into which to store the file.
/// \param path Path to the file to be stored.
///
/// \return The identifier of the stored file, or none if the file was empty.
///
/// \throw sqlite::error If there are problems writing to the database.
static optional< int64_t >
put_file(sqlite::database& db, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        throw store::error(F("Cannot open file %s") % path);

    try {
        if (utils::stream_length(input) == 0)
            return none;
    } catch (const std::runtime_error& e) {
        // Skipping empty files is an optimization.  If we fail to calculate the
        // size of the file, just ignore the problem.  If there are real issues
        // with the file, the read below will fail anyway.
        LD(F("Cannot determine if file is empty: %s") % e.what());
    }

    // TODO(jmmv): This will probably cause an unreasonable amount of memory
    // consumption if we decide to store arbitrary files in the database (other
    // than stdout or stderr).  Should this happen, we need to investigate a
    // better way to feel blobs into SQLite.
    const std::string contents = utils::read_stream(input);

    sqlite::statement stmt = db.create_statement(
        "INSERT INTO files (contents) VALUES (:contents)");
    stmt.bind(":contents", sqlite::blob(contents.c_str(), contents.length()));
    stmt.step_without_results();

    return optional< int64_t >(db.last_insert_rowid());
}


}  // anonymous namespace


/// Loads a specific test program from the database.
///
/// \param backend_ The store backend we are dealing with.
/// \param id The identifier of the test program to load.
///
/// \return The instantiated test program.
///
/// \throw integrity_error If the data read from the database cannot be properly
///     interpreted.
engine::test_program_ptr
store::detail::get_test_program(backend& backend_, const int64_t id)
{
    sqlite::database& db = backend_.database();

    engine::test_program_ptr test_program;
    sqlite::statement stmt = db.create_statement(
        "SELECT * FROM test_programs WHERE test_program_id == :id");
    stmt.bind(":id", id);
    stmt.step();
    const std::string interface = stmt.safe_column_text("interface");
    test_program.reset(new engine::test_program(
        interface,
        fs::path(stmt.safe_column_text("relative_path")),
        fs::path(stmt.safe_column_text("root")),
        stmt.safe_column_text("test_suite_name"),
        get_metadata(db, stmt.safe_column_int64("metadata_id"))));
    const bool more = stmt.step();
    INV(!more);

    LD(F("Loaded test program '%s'; getting test cases") %
       test_program->relative_path());
    test_program->set_test_cases(get_test_cases(db, id, *test_program,
                                                interface));
    return test_program;
}


/// Internal implementation for a results iterator.
struct store::results_iterator::impl {
    /// The store backend we are dealing with.
    store::backend _backend;

    /// The statement to iterate on.
    sqlite::statement _stmt;

    /// A cache for the last loaded test program.
    optional< std::pair< int64_t, engine::test_program_ptr > >
        _last_test_program;

    /// Whether the iterator is still valid or not.
    bool _valid;

    /// Constructor.
    impl(store::backend& backend_, const int64_t action_id_) :
        _backend(backend_),
        _stmt(backend_.database().create_statement(
            "SELECT test_programs.test_program_id, "
            "    test_programs.interface, "
            "    test_cases.test_case_id, test_cases.name, "
            "    test_results.result_type, test_results.result_reason, "
            "    test_results.start_time, test_results.end_time "
            "FROM test_programs "
            "    JOIN test_cases "
            "    ON test_programs.test_program_id = test_cases.test_program_id "
            "    JOIN test_results "
            "    ON test_cases.test_case_id = test_results.test_case_id "
            "WHERE test_programs.action_id == :action_id "
            "ORDER BY test_programs.absolute_path, test_cases.name"))
    {
        _stmt.bind(":action_id", action_id_);
        _valid = _stmt.step();
    }
};


/// Constructor.
///
/// \param pimpl_ The internal implementation details of the iterator.
store::results_iterator::results_iterator(
    std::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::results_iterator::~results_iterator(void)
{
}


/// Moves the iterator forward by one result.
///
/// \return The iterator itself.
store::results_iterator&
store::results_iterator::operator++(void)
{
    _pimpl->_valid = _pimpl->_stmt.step();
    return *this;
}


/// Checks whether the iterator is still valid.
///
/// \return True if there is more elements to iterate on, false otherwise.
store::results_iterator::operator bool(void) const
{
    return _pimpl->_valid;
}


/// Gets the test program this result belongs to.
///
/// \return The representation of a test program.
const engine::test_program_ptr
store::results_iterator::test_program(void) const
{
    const int64_t id = _pimpl->_stmt.safe_column_int64("test_program_id");
    if (!_pimpl->_last_test_program ||
        _pimpl->_last_test_program.get().first != id)
    {
        const engine::test_program_ptr tp = detail::get_test_program(
            _pimpl->_backend, id);
        _pimpl->_last_test_program = std::make_pair(id, tp);
    }
    return _pimpl->_last_test_program.get().second;
}


/// Gets the name of the test case pointed by the iterator.
///
/// The caller can look up the test case data by using the find() method on the
/// test program returned by test_program().
///
/// \return A test case name, unique within the test program.
std::string
store::results_iterator::test_case_name(void) const
{
    return _pimpl->_stmt.safe_column_text("name");
}


/// Gets the result of the test case pointed by the iterator.
///
/// \return A test case result.
engine::test_result
store::results_iterator::result(void) const
{
    return parse_result(_pimpl->_stmt, "result_type", "result_reason");
}


/// Gets the duration of the test case execution.
///
/// \return A time delta representing the run time of the test case.
datetime::delta
store::results_iterator::duration(void) const
{
    const datetime::timestamp start_time = column_timestamp(
        _pimpl->_stmt, "start_time");
    const datetime::timestamp end_time = column_timestamp(
        _pimpl->_stmt, "end_time");
    return end_time - start_time;
}


/// Gets a file from a test case.
///
/// \param db The database to query the file from.
/// \param test_case_id The identifier of the test case.
/// \param filename The name of the file to be retrieved.
///
/// \return A textual representation of the file contents.
///
/// \throw integrity_error If there is any problem in the loaded data or if the
///     file cannot be found.
static std::string
get_test_case_file(sqlite::database& db, const int64_t test_case_id,
                   const char* filename)
{
    sqlite::statement stmt = db.create_statement(
        "SELECT file_id FROM test_case_files "
        "WHERE test_case_id == :test_case_id AND file_name == :file_name");
    stmt.bind(":test_case_id", test_case_id);
    stmt.bind(":file_name", filename);
    if (stmt.step())
        return get_file(db, stmt.safe_column_int64("file_id"));
    else
        return "";
}


/// Gets the contents of stdout of a test case.
///
/// \return A textual representation of the stdout contents of the test case.
/// This may of course be empty if the test case didn't print anything.
std::string
store::results_iterator::stdout_contents(void) const
{
    return get_test_case_file(_pimpl->_backend.database(),
                              _pimpl->_stmt.safe_column_int64("test_case_id"),
                              "__STDOUT__");
}


/// Gets the contents of stderr of a test case.
///
/// \return A textual representation of the stderr contents of the test case.
/// This may of course be empty if the test case didn't print anything.
std::string
store::results_iterator::stderr_contents(void) const
{
    return get_test_case_file(_pimpl->_backend.database(),
                              _pimpl->_stmt.safe_column_int64("test_case_id"),
                              "__STDERR__");
}


/// Internal implementation for a store transaction.
struct store::transaction::impl {
    /// The backend instance.
    store::backend& _backend;


    /// The SQLite database this transaction deals with.
    sqlite::database _db;

    /// The backing SQLite transaction.
    sqlite::transaction _tx;

    /// Opens a transaction.
    ///
    /// \param backend_ The backend this transaction is connected to.
    impl(backend& backend_) :
        _backend(backend_),
        _db(backend_.database()),
        _tx(backend_.database().begin_transaction())
    {
    }
};


/// Creates a new transaction.
///
/// \param backend_ The backend this transaction belongs to.
store::transaction::transaction(backend& backend_) :
    _pimpl(new impl(backend_))
{
}


/// Destructor.
store::transaction::~transaction(void)
{
}


/// Commits the transaction.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::commit(void)
{
    try {
        _pimpl->_tx.commit();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Rolls the transaction back.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::rollback(void)
{
    try {
        _pimpl->_tx.rollback();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Retrieves an action from the database.
///
/// \param action_id The identifier of the action to retrieve.
///
/// \return The retrieved action.
///
/// \throw error If there is a problem loading the action.
engine::action
store::transaction::get_action(const int64_t action_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT context_id FROM actions WHERE action_id == :action_id");
        stmt.bind(":action_id", action_id);
        if (!stmt.step())
            throw error(F("Error loading action %s: does not exist") %
                        action_id);

        return engine::action(
            get_context(stmt.safe_column_int64("context_id")));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading action %s: %s") % action_id % e.what());
    }
}


/// Creates a new iterator to scan the test results of an action.
///
/// \param action_id The identifier of the action for which to get the results.
///
/// \return The constructed iterator.
///
/// \throw error If there is any problem constructing the iterator.
store::results_iterator
store::transaction::get_action_results(const int64_t action_id)
{
    try {
        return results_iterator(std::shared_ptr< results_iterator::impl >(
           new results_iterator::impl(_pimpl->_backend, action_id)));
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Retrieves the latest action from the database.
///
/// \return The retrieved action.
///
/// \throw error If there is a problem loading the action.
std::pair< int64_t, engine::action >
store::transaction::get_latest_action(void)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT action_id, context_id FROM actions WHERE "
            "action_id == (SELECT max(action_id) FROM actions)");
        if (!stmt.step())
            throw error("No actions in the database");

        const int64_t action_id = stmt.safe_column_int64("action_id");
        const engine::context context = get_context(
            stmt.safe_column_int64("context_id"));

        return std::pair< int64_t, engine::action >(
            action_id, engine::action(context));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading latest action: %s") % e.what());
    }
}


/// Retrieves an context from the database.
///
/// \param context_id The identifier of the context to retrieve.
///
/// \return The retrieved context.
///
/// \throw error If there is a problem loading the context.
engine::context
store::transaction::get_context(const int64_t context_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT cwd FROM contexts WHERE context_id == :context_id");
        stmt.bind(":context_id", context_id);
        if (!stmt.step())
            throw error(F("Error loading context %s: does not exist") %
                        context_id);

        return engine::context(fs::path(stmt.safe_column_text("cwd")),
                               get_env_vars(_pimpl->_db, context_id));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading context %s: %s") % context_id % e.what());
    }
}


/// Puts an action into the database.
///
/// \pre The action has not been put yet.
/// \pre The dependent objects have already been put.
/// \post The action is stored into the database with a new identifier.
///
/// \param unused_action The action to put.
/// \param context_id The identifier for the action's context.
///
/// \return The identifier of the inserted action.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_action(const engine::action& UTILS_UNUSED_PARAM(action),
                               const int64_t context_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO actions (context_id) VALUES (:context_id)");
        stmt.bind(":context_id", context_id);
        stmt.step_without_results();
        const int64_t action_id = _pimpl->_db.last_insert_rowid();

        return action_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a context into the database.
///
/// \pre The context has not been put yet.
/// \post The context is stored into the database with a new identifier.
///
/// \param context The context to put.
///
/// \return The identifier of the inserted context.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_context(const engine::context& context)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO contexts (cwd) VALUES (:cwd)");
        stmt.bind(":cwd", context.cwd().str());
        stmt.step_without_results();
        const int64_t context_id = _pimpl->_db.last_insert_rowid();

        put_env_vars(_pimpl->_db, context_id, context.env());

        return context_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a test program into the database.
///
/// \pre The test program has not been put yet.
/// \post The test program is stored into the database with a new identifier.
///
/// \param test_program The test program to put.
/// \param action_id The action this test program belongs to.
///
/// \return The identifier of the inserted test program.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_test_program(const engine::test_program& test_program,
                                     const int64_t action_id)
{
    try {
        const int64_t metadata_id = put_metadata(
            _pimpl->_db, test_program.get_metadata());

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_programs (action_id, absolute_path, "
            "                           root, relative_path, test_suite_name, "
            "                           metadata_id, interface) "
            "VALUES (:action_id, :absolute_path, :root, :relative_path, "
            "        :test_suite_name, :metadata_id, :interface)");
        stmt.bind(":action_id", action_id);
        stmt.bind(":absolute_path", test_program.absolute_path().str());
        // TODO(jmmv): The root is not necessarily absolute.  We need to ensure
        // that we can recover the absolute path of the test program.  Maybe we
        // need to change the test_program to always ensure root is absolute?
        stmt.bind(":root", test_program.root().str());
        stmt.bind(":relative_path", test_program.relative_path().str());
        stmt.bind(":test_suite_name", test_program.test_suite_name());
        stmt.bind(":metadata_id", metadata_id);
        stmt.bind(":interface", test_program.interface_name());
        stmt.step_without_results();
        return _pimpl->_db.last_insert_rowid();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a test case into the database.
///
/// \pre The test case has not been put yet.
/// \post The test case is stored into the database with a new identifier.
///
/// \param test_case The test case to put.
/// \param test_program_id The test program this test case belongs to.
///
/// \return The identifier of the inserted test case.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_test_case(const engine::test_case& test_case,
                                  const int64_t test_program_id)
{
    try {
        const int64_t metadata_id = put_metadata(
            _pimpl->_db, test_case.get_metadata());

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_cases (test_program_id, name, metadata_id) "
            "VALUES (:test_program_id, :name, :metadata_id)");
        stmt.bind(":test_program_id", test_program_id);
        stmt.bind(":name", test_case.name());
        stmt.bind(":metadata_id", metadata_id);
        stmt.step_without_results();
        return _pimpl->_db.last_insert_rowid();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Stores a file generated by a test case into the database as a BLOB.
///
/// \param name The name of the file to store in the database.  This needs to be
///     unique per test case.  The caller is free to decide what names to use
///     for which files.  For example, it might make sense to always call
///     __STDOUT__ the stdout of the test case so that it is easy to locate.
/// \param path The path to the file to be stored.
/// \param test_case_id The identifier of the test case this file belongs to.
///
/// \return The identifier of the stored file, or none if the file was empty.
///
/// \throw store::error If there are problems writing to the database.
optional< int64_t >
store::transaction::put_test_case_file(const std::string& name,
                                       const fs::path& path,
                                       const int64_t test_case_id)
{
    LD(F("Storing %s (%s) of test case %s") % name % path % test_case_id);
    try {
        const optional< int64_t > file_id = put_file(_pimpl->_db, path);
        if (!file_id) {
            LD("Not storing empty file");
            return none;
        }

        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_case_files (test_case_id, file_name, file_id) "
            "VALUES (:test_case_id, :file_name, :file_id)");
        stmt.bind(":test_case_id", test_case_id);
        stmt.bind(":file_name", name);
        stmt.bind(":file_id", file_id.get());
        stmt.step_without_results();

        return optional< int64_t >(_pimpl->_db.last_insert_rowid());
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a result into the database.
///
/// \pre The result has not been put yet.
/// \post The result is stored into the database with a new identifier.
///
/// \param result The result to put.
/// \param test_case_id The test case this result corresponds to.
/// \param start_time The time when the test started to run.
/// \param end_time The time when the test finished running.
///
/// \return The identifier of the inserted result.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_result(const engine::test_result& result,
                               const int64_t test_case_id,
                               const datetime::timestamp& start_time,
                               const datetime::timestamp& end_time)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_results (test_case_id, result_type, "
            "                          result_reason, start_time, "
            "                          end_time) "
            "VALUES (:test_case_id, :result_type, :result_reason, "
            "        :start_time, :end_time)");
        stmt.bind(":test_case_id", test_case_id);

        switch (result.type()) {
        case engine::test_result::broken:
            stmt.bind(":result_type", "broken");
            break;

        case engine::test_result::expected_failure:
            stmt.bind(":result_type", "expected_failure");
            break;

        case engine::test_result::failed:
            stmt.bind(":result_type", "failed");
            break;

        case engine::test_result::passed:
            stmt.bind(":result_type", "passed");
            break;

        case engine::test_result::skipped:
            stmt.bind(":result_type", "skipped");
            break;

        default:
            UNREACHABLE;
        }

        if (result.reason().empty())
            stmt.bind(":result_reason", sqlite::null());
        else
            stmt.bind(":result_reason", result.reason());

        store::bind_timestamp(stmt, ":start_time", start_time);
        store::bind_timestamp(stmt, ":end_time", end_time);

        stmt.step_without_results();
        const int64_t result_id = _pimpl->_db.last_insert_rowid();

        return result_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
