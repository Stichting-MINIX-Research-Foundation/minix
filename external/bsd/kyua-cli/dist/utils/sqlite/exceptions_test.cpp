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

#include "utils/sqlite/exceptions.hpp"

#include <cstring>

#include <atf-c++.hpp>

#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/database.hpp"

namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const sqlite::error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__explicit);
ATF_TEST_CASE_BODY(api_error__explicit)
{
    const sqlite::api_error e("some_function", "Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
    ATF_REQUIRE_EQ("some_function", e.api_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__from_database);
ATF_TEST_CASE_BODY(api_error__from_database)
{
    ::sqlite3* raw_db;
    ATF_REQUIRE_EQ(SQLITE_CANTOPEN, ::sqlite3_open_v2("missing.db", &raw_db,
        SQLITE_OPEN_READONLY, NULL));

    sqlite::database gate = sqlite::database_c_gate::connect(raw_db);
    const sqlite::api_error e = sqlite::api_error::from_database(
        gate, "real_function");
    ATF_REQUIRE(std::strcmp("unable to open database file", e.what()) == 0);
    ATF_REQUIRE_EQ("real_function", e.api_function());

    ::sqlite3_close(raw_db);
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_column_error);
ATF_TEST_CASE_BODY(invalid_column_error)
{
    const sqlite::invalid_column_error e("some_name");
    ATF_REQUIRE(std::strcmp("Unknown column 'some_name'", e.what()) == 0);
    ATF_REQUIRE_EQ("some_name", e.column_name());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);

    ATF_ADD_TEST_CASE(tcs, api_error__explicit);
    ATF_ADD_TEST_CASE(tcs, api_error__from_database);

    ATF_ADD_TEST_CASE(tcs, invalid_column_error);
}
