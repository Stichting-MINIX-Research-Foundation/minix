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

#include "cli/cmd_test.hpp"

#include <cstdlib>

#include "cli/common.ipp"
#include "engine/drivers/run_tests.hpp"
#include "engine/test_case.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace run_tests = engine::drivers::run_tests;

using cli::cmd_test;


namespace {


/// Hooks to print a progress report of the execution of the tests.
class print_hooks : public run_tests::base_hooks {
    /// Object to interact with the I/O of the program.
    cmdline::ui* _ui;

public:
    /// The amount of positive test results found so far.
    unsigned long good_count;

    /// The amount of negative test results found so far.
    unsigned long bad_count;

    /// Constructor for the hooks.
    ///
    /// \param ui_ Object to interact with the I/O of the program.
    print_hooks(cmdline::ui* ui_) :
        _ui(ui_),
        good_count(0),
        bad_count(0)
    {
    }

    /// Called when the processing of a test case begins.
    ///
    /// \param test_case The test case.
    virtual void
    got_test_case(const engine::test_case_ptr& test_case)
    {
        _ui->out(F("%s  ->  ") % cli::format_test_case_id(*test_case), false);
    }

    /// Called when a result of a test case becomes available.
    ///
    /// \param unused_test_case The test case.
    /// \param result The result of the execution of the test case.
    /// \param duration The time it took to run the test.
    virtual void
    got_result(const engine::test_case_ptr& UTILS_UNUSED_PARAM(test_case),
               const engine::test_result& result,
               const datetime::delta& duration)
    {
        _ui->out(F("%s  [%s]") % cli::format_result(result) %
            cli::format_delta(duration));
        if (result.good())
            good_count++;
        else
            bad_count++;
    }
};


}  // anonymous namespace


/// Default constructor for cmd_test.
cmd_test::cmd_test(void) : cli_command(
    "test", "[test-program ...]", 0, -1, "Run tests")
{
    add_option(build_root_option);
    add_option(kyuafile_option);
    add_option(store_option);
}


/// Entry point for the "test" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 if all tests passed, 1 otherwise.
int
cmd_test::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
              const config::tree& user_config)
{
    print_hooks hooks(ui);
    const run_tests::result result = run_tests::drive(
        kyuafile_path(cmdline), build_root_path(cmdline), store_path(cmdline),
        parse_filters(cmdline.arguments()), user_config, hooks);

    int exit_code;
    if (hooks.good_count > 0 || hooks.bad_count > 0) {
        ui->out("");
        ui->out(F("%s/%s passed (%s failed)") % hooks.good_count %
                (hooks.good_count + hooks.bad_count) % hooks.bad_count);

        exit_code = (hooks.bad_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    } else
        exit_code = EXIT_SUCCESS;

    ui->out(F("Committed action %s") % result.action_id);

    return report_unused_filters(result.unused_filters, ui) ?
        EXIT_FAILURE : exit_code;
}
