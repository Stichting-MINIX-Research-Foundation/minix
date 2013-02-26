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

#include "store/backend.hpp"

#include <fstream>

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/transaction.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


/// The current schema version.
///
/// Any new database gets this schema version.  Existing databases with an older
/// schema version must be first migrated to the current schema with
/// migrate_schema() before they can be used.
///
/// This must be kept in sync with the value in the corresponding schema_vX.sql
/// file, where X matches this version number.
///
/// This variable is not const to allow tests to modify it.  No other code
/// should change its value.
int store::detail::current_schema_version = 2;


namespace {


/// Opens a database and defines session pragmas.
///
/// This auxiliary function ensures that, every time we open a SQLite database,
/// we define the same set of pragmas for it.
///
/// \param file The database file to be opened.
/// \param flags The flags for the open; see sqlite::database::open.
///
/// \return The opened database.
///
/// \throw store::error If there is a problem opening or creating the database.
static sqlite::database
do_open(const fs::path& file, const int flags)
{
    try {
        sqlite::database database = sqlite::database::open(file, flags);
        database.exec("PRAGMA foreign_keys = ON");
        return database;
    } catch (const sqlite::error& e) {
        throw store::error(F("Cannot open '%s': %s") % file % e.what());
    }
}


/// Checks if a database is empty (i.e. if it is new).
///
/// \param db The database to check.
///
/// \return True if the database is empty.
static bool
empty_database(sqlite::database& db)
{
    sqlite::statement stmt = db.create_statement("SELECT * FROM sqlite_master");
    return !stmt.step();
}


/// Performs a single migration step.
///
/// \param db Open database to which to apply the migration step.
/// \param version_from Current schema version in the database.
/// \param version_to Schema version to migrate to.
///
/// \throw error If there is a problem applying the migration.
static void
migrate_schema_step(sqlite::database& db, const int version_from,
                    const int version_to)
{
    PRE(version_to == version_from + 1);

    const fs::path migration = store::detail::migration_file(version_from,
                                                             version_to);

    std::ifstream input(migration.c_str());
    if (!input)
        throw store::error(F("Cannot open migration file '%s'") % migration);

    const std::string migration_string = utils::read_stream(input);
    try {
        db.exec(migration_string);
    } catch (const sqlite::error& e) {
        throw store::error(F("Schema migration failed: %s") % e.what());
    }
}


}  // anonymous namespace


/// Calculates the path to a schema migration file.
///
/// \param version_from The version from which the database is being upgraded.
/// \param version_to The version to which the database is being upgraded.
///
/// \return The path to the installed migrate_vX_vY.sql file.
fs::path
store::detail::migration_file(const int version_from, const int version_to)
{
    return fs::path(utils::getenv_with_default("KYUA_STOREDIR", KYUA_STOREDIR))
        / (F("migrate_v%s_v%s.sql") % version_from % version_to);
}


/// Calculates the path to the schema file for the database.
///
/// \return The path to the installed schema_vX.sql file that matches the
/// current_schema_version.
fs::path
store::detail::schema_file(void)
{
    return fs::path(utils::getenv_with_default("KYUA_STOREDIR", KYUA_STOREDIR))
        / (F("schema_v%s.sql") % current_schema_version);
}


/// Initializes an empty database.
///
/// \param db The database to initialize.
///
/// \return The metadata record written into the new database.
///
/// \throw store::error If there is a problem initializing the database.
store::metadata
store::detail::initialize(sqlite::database& db)
{
    PRE(empty_database(db));

    const fs::path schema = schema_file();

    std::ifstream input(schema.c_str());
    if (!input)
        throw error(F("Cannot open database schema '%s'") % schema);

    LI(F("Populating new database with schema from %s") % schema);
    const std::string schema_string = utils::read_stream(input);
    try {
        db.exec(schema_string);

        const metadata metadata = metadata::fetch_latest(db);
        LI(F("New metadata entry %s") % metadata.timestamp());
        if (metadata.schema_version() != detail::current_schema_version) {
            UNREACHABLE_MSG(F("current_schema_version is out of sync with "
                              "%s") % schema);
        }
        return metadata;
    } catch (const store::integrity_error& e) {
        // Could be raised by metadata::fetch_latest.
        UNREACHABLE_MSG("Inconsistent code while creating a database");
    } catch (const sqlite::error& e) {
        throw error(F("Failed to initialize database: %s") % e.what());
    }
}


/// Backs up a database for schema migration purposes.
///
/// \todo We should probably use the SQLite backup API instead of doing a raw
/// file copy.  We issue our backup call with the database already open, but
/// because it is quiescent, it's OK to do so.
///
/// \param source Location of the database to be backed up.
/// \param old_version Version of the database's CURRENT schema, used to
///     determine the name of the backup file.
///
/// \throw error If there is a problem during the backup.
void
store::detail::backup_database(const fs::path& source, const int old_version)
{
    const fs::path target(F("%s.v%s.backup") % source.str() % old_version);

    LI(F("Backing up database %s to %s") % source % target);

    std::ifstream input(source.c_str());
    if (!input)
        throw error(F("Cannot open database file %s") % source);

    std::ofstream output(target.c_str());
    if (!output)
        throw error(F("Cannot create database backup file %s") % target);

    char buffer[1024];
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        if (input.good() || input.eof())
            output.write(buffer, input.gcount());
    }
    if (!input.good() && !input.eof())
        throw error(F("Error while reading input file %s") % source);
}


/// Internal implementation for the backend.
struct store::backend::impl {
    /// The SQLite database this backend talks to.
    sqlite::database database;

    /// Constructor.
    ///
    /// \param database_ The SQLite database instance.
    /// \param metadata_ The metadata for the loaded database.  This must match
    ///     the schema version we implement in this module; otherwise, a
    ///     migration is necessary.
    ///
    /// \throw integrity_error If the schema in the database is too modern,
    ///     which might indicate some form of corruption or an old binary.
    /// \throw old_schema_error If the schema in the database is older than our
    ///     currently-implemented version and needs an upgrade.  The caller can
    ///     use migrate_schema() to fix this problem.
    impl(sqlite::database& database_, const metadata& metadata_) :
        database(database_)
    {
        const int database_version = metadata_.schema_version();

        if (database_version == detail::current_schema_version) {
            // OK.
        } else if (database_version < detail::current_schema_version) {
            throw old_schema_error(database_version);
        } else if (database_version > detail::current_schema_version) {
            throw integrity_error(
                F("Database at schema version %s, which is newer than the "
                  "supported version %s")
                % database_version % detail::current_schema_version);
        }
    }
};


/// Constructs a new backend.
///
/// \param pimpl_ The internal data.
store::backend::backend(impl* pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::backend::~backend(void)
{
}


/// Opens a database in read-only mode.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening the database.
store::backend
store::backend::open_ro(const fs::path& file)
{
    sqlite::database db = do_open(file, sqlite::open_readonly);
    return backend(new impl(db, metadata::fetch_latest(db)));
}


/// Opens a database in read-write mode and creates it if necessary.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening or creating
///     the database.
store::backend
store::backend::open_rw(const fs::path& file)
{
    sqlite::database db = do_open(file, sqlite::open_readwrite |
                                  sqlite::open_create);
    if (empty_database(db))
        return backend(new impl(db, detail::initialize(db)));
    else
        return backend(new impl(db, metadata::fetch_latest(db)));
}


/// Gets the connection to the SQLite database.
///
/// \return A database connection.
sqlite::database&
store::backend::database(void)
{
    return _pimpl->database;
}


/// Opens a transaction.
///
/// \return A new transaction.
store::transaction
store::backend::start(void)
{
    return transaction(*this);
}


/// Migrates the schema of a database to the current version.
///
/// The algorithm implemented here performs a migration step for every
/// intermediate version between the schema version in the database to the
/// version implemented in this file.  This should permit upgrades from
/// arbitrary old databases.
///
/// \param file The database whose schema to upgrade.
///
/// \throw error If there is a problem with the migration.
void
store::migrate_schema(const utils::fs::path& file)
{
    sqlite::database db = do_open(file, sqlite::open_readwrite);

    const int version_from = metadata::fetch_latest(db).schema_version();
    const int version_to = detail::current_schema_version;
    if (version_from == version_to) {
        throw error(F("Database already at schema version %s; migration not "
                      "needed") % version_from);
    } else if (version_from > version_to) {
        throw error(F("Database at schema version %s, which is newer than the "
                      "supported version %s") % version_from % version_to);
    }

    detail::backup_database(file, version_from);

    for (int i = version_from; i < version_to; ++i) {
        LI(F("Migrating schema from version %s to %s") % i % (i + 1));
        migrate_schema_step(db, i, i + 1);
    }
}
