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

#include "cli/cmd_report.hpp"

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/ui_mock.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(file_writer__stdout);
ATF_TEST_CASE_BODY(file_writer__stdout)
{
    cmdline::ui_mock ui;
    {
        cli::file_writer writer(&ui, fs::path("/dev/stdout"));
        writer("A simple message");
    }

    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("A simple message", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(file_writer__stderr);
ATF_TEST_CASE_BODY(file_writer__stderr)
{
    cmdline::ui_mock ui;
    {
        cli::file_writer writer(&ui, fs::path("/dev/stderr"));
        writer("A simple message");
    }

    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("A simple message", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(file_writer__other);
ATF_TEST_CASE_BODY(file_writer__other)
{
    cmdline::ui_mock ui;
    {
        cli::file_writer writer(&ui, fs::path("custom"));
        writer("A simple message");
    }

    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
    ATF_REQUIRE(atf::utils::grep_file("A simple message", "custom"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, file_writer__stdout);
    ATF_ADD_TEST_CASE(tcs, file_writer__stderr);
    ATF_ADD_TEST_CASE(tcs, file_writer__other);
}
