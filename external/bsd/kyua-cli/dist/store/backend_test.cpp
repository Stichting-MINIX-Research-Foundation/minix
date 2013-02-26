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

extern "C" {
#include <sys/stat.h>
}

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__ok);
ATF_TEST_CASE_BODY(detail__backup_database__ok)
{
    atf::utils::create_file("test.db", "The DB\n");
    store::detail::backup_database(fs::path("test.db"), 13);
    ATF_REQUIRE(fs::exists(fs::path("test.db")));
    ATF_REQUIRE(fs::exists(fs::path("test.db.v13.backup")));
    ATF_REQUIRE(atf::utils::compare_file("test.db.v13.backup", "The DB\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__ok_overwrite);
ATF_TEST_CASE_BODY(detail__backup_database__ok_overwrite)
{
    atf::utils::create_file("test.db", "Original contents");
    atf::utils::create_file("test.db.v1.backup", "Overwrite me");
    store::detail::backup_database(fs::path("test.db"), 1);
    ATF_REQUIRE(fs::exists(fs::path("test.db")));
    ATF_REQUIRE(fs::exists(fs::path("test.db.v1.backup")));
    ATF_REQUIRE(atf::utils::compare_file("test.db.v1.backup",
                                         "Original contents"));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__backup_database__fail_open);
ATF_TEST_CASE_BODY(detail__backup_database__fail_open)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open.*foo.db",
                         store::detail::backup_database(fs::path("foo.db"), 5));
}


ATF_TEST_CASE(detail__backup_database__fail_create);
ATF_TEST_CASE_HEAD(detail__backup_database__fail_create)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(detail__backup_database__fail_create)
{
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    atf::utils::create_file("dir/test.db", "Does not need to be valid");
    ATF_REQUIRE(::chmod("dir", 0111) != -1);
    ATF_REQUIRE_THROW_RE(
        store::error, "Cannot create.*dir/test.db.v13.backup",
        store::detail::backup_database(fs::path("dir/test.db"), 13));
}


ATF_TEST_CASE(detail__initialize__ok);
ATF_TEST_CASE_HEAD(detail__initialize__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(detail__initialize__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    const datetime::timestamp before = datetime::timestamp::now();
    const store::metadata md = store::detail::initialize(db);
    const datetime::timestamp after = datetime::timestamp::now();

    ATF_REQUIRE(md.timestamp() >= before.to_seconds());
    ATF_REQUIRE(md.timestamp() <= after.to_microseconds());
    ATF_REQUIRE_EQ(store::detail::current_schema_version, md.schema_version());

    // Query some known tables to ensure they were created.
    db.exec("SELECT * FROM metadata");

    // And now query some know values.
    sqlite::statement stmt = db.create_statement(
        "SELECT COUNT(*) FROM metadata");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(1, stmt.column_int(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__initialize__missing_schema);
ATF_TEST_CASE_BODY(detail__initialize__missing_schema)
{
    utils::setenv("KYUA_STOREDIR", "/non-existent");
    store::detail::current_schema_version = 712;

    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error,
                         "Cannot open.*'/non-existent/schema_v712.sql'",
                         store::detail::initialize(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__initialize__sqlite_error);
ATF_TEST_CASE_BODY(detail__initialize__sqlite_error)
{
    utils::setenv("KYUA_STOREDIR", ".");
    store::detail::current_schema_version = 712;

    atf::utils::create_file("schema_v712.sql", "foo_bar_baz;\n");

    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error, "Failed to initialize.*:.*foo_bar_baz",
                         store::detail::initialize(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__migration_file__builtin);
ATF_TEST_CASE_BODY(detail__migration_file__builtin)
{
    utils::unsetenv("KYUA_STOREDIR");
    ATF_REQUIRE_EQ(fs::path(KYUA_STOREDIR) / "migrate_v5_v9.sql",
                   store::detail::migration_file(5, 9));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__migration_file__overriden);
ATF_TEST_CASE_BODY(detail__migration_file__overriden)
{
    utils::setenv("KYUA_STOREDIR", "/tmp/test");
    ATF_REQUIRE_EQ(fs::path("/tmp/test/migrate_v5_v9.sql"),
                   store::detail::migration_file(5, 9));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__schema_file__builtin);
ATF_TEST_CASE_BODY(detail__schema_file__builtin)
{
    utils::unsetenv("KYUA_STOREDIR");
    ATF_REQUIRE_EQ(fs::path(KYUA_STOREDIR) / "schema_v2.sql",
                   store::detail::schema_file());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__schema_file__overriden);
ATF_TEST_CASE_BODY(detail__schema_file__overriden)
{
    utils::setenv("KYUA_STOREDIR", "/tmp/test");
    store::detail::current_schema_version = 123;
    ATF_REQUIRE_EQ(fs::path("/tmp/test/schema_v123.sql"),
                   store::detail::schema_file());
}


ATF_TEST_CASE(backend__open_ro__ok);
ATF_TEST_CASE_HEAD(backend__open_ro__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(backend__open_ro__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE_WITHOUT_HEAD(backend__open_ro__missing_file);
ATF_TEST_CASE_BODY(backend__open_ro__missing_file)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open 'missing.db': ",
                         store::backend::open_ro(fs::path("missing.db")));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE(backend__open_ro__integrity_error);
ATF_TEST_CASE_HEAD(backend__open_ro__integrity_error)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(backend__open_ro__integrity_error)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
        db.exec("DELETE FROM metadata");
    }
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::backend::open_ro(fs::path("test.db")));
}


ATF_TEST_CASE(backend__open_rw__ok);
ATF_TEST_CASE_HEAD(backend__open_rw__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(backend__open_rw__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE(backend__open_rw__create_missing);
ATF_TEST_CASE_HEAD(backend__open_rw__create_missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(backend__open_rw__create_missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE(backend__open_rw__integrity_error);
ATF_TEST_CASE_HEAD(backend__open_rw__integrity_error)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(backend__open_rw__integrity_error)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
        db.exec("DELETE FROM metadata");
    }
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::backend::open_rw(fs::path("test.db")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__ok);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__ok_overwrite);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__fail_open);
    ATF_ADD_TEST_CASE(tcs, detail__backup_database__fail_create);

    ATF_ADD_TEST_CASE(tcs, detail__initialize__ok);
    ATF_ADD_TEST_CASE(tcs, detail__initialize__missing_schema);
    ATF_ADD_TEST_CASE(tcs, detail__initialize__sqlite_error);

    ATF_ADD_TEST_CASE(tcs, detail__migration_file__builtin);
    ATF_ADD_TEST_CASE(tcs, detail__migration_file__overriden);

    ATF_ADD_TEST_CASE(tcs, detail__schema_file__builtin);
    ATF_ADD_TEST_CASE(tcs, detail__schema_file__overriden);

    ATF_ADD_TEST_CASE(tcs, backend__open_ro__ok);
    ATF_ADD_TEST_CASE(tcs, backend__open_ro__missing_file);
    ATF_ADD_TEST_CASE(tcs, backend__open_ro__integrity_error);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__ok);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__create_missing);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__integrity_error);

    // Tests for migrate_schema are in schema_inttest.  This is because, for
    // such tests to be meaningful, they need to be integration tests and don't
    // really fit the goal of this unit-test module.
}
