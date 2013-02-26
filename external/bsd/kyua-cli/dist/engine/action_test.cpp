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

#include "engine/action.hpp"

#include <map>
#include <sstream>
#include <string>

#include <atf-c++.hpp>

#include "engine/context.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


namespace {


/// Generates a context with fake data for testing purposes only.
///
/// \param cwd The work directory held in the context.  This contains an
///     irrelevant default value if not provided.
///
/// \return The fake context.
static engine::context
fake_context(const char* cwd = "/foo/bar")
{
    std::map< std::string, std::string > env;
    env["foo"] = "bar";
    return engine::context(fs::path(cwd), env);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const engine::context context = fake_context();
    const engine::action action(context);
    ATF_REQUIRE(context == action.runtime_context());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne);
ATF_TEST_CASE_BODY(operators_eq_and_ne)
{
    const engine::action action1(fake_context("foo/bar"));
    const engine::action action2(fake_context("foo/bar"));
    const engine::action action3(fake_context("foo/baz"));
    ATF_REQUIRE(  action1 == action2);
    ATF_REQUIRE(!(action1 != action2));
    ATF_REQUIRE(!(action1 == action3));
    ATF_REQUIRE(  action1 != action3);
}


ATF_TEST_CASE_WITHOUT_HEAD(output);
ATF_TEST_CASE_BODY(output)
{
    const engine::context context = fake_context();
    const engine::action action(context);

    std::ostringstream str;
    str << action;
    ATF_REQUIRE_EQ("action{context=context{cwd='/foo/bar', env=[foo='bar']}}",
                   str.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne);
    ATF_ADD_TEST_CASE(tcs, output);
}
