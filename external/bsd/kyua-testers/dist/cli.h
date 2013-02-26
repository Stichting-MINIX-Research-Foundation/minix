// Copyright 2012 Google Inc.
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

/// \file cli.h
/// Generic command-line implementation of a tester.
///
/// The cli module implements a generic command-line for a tester
/// regardless of the test interface it implements.  This command-line must
/// be consistent across all implementations so that the Kyua runtime does
/// not need to have any knowledge of the specific test interfaces.

#if !defined(KYUA_CLI_H)
#define KYUA_CLI_H

#include "cli_fwd.h"

#include <stdbool.h>

#include "error_fwd.h"
#include "run_fwd.h"


// Error codes returned by the CLI are as follows.  These match the rationale of
// the return values of the kyua(1) tool.
//
// EXIT_SUCCESS -> The command and the tester ran correctly.
// EXIT_FAILURE -> The command reported an error, but the tester ran correctly.
//     For example: the test command reports that the test itself failed, but
//     nothing in the tester misbehaved.
// EXIT_INTERNAL_ERROR -> The tester itself failed.
// EXIT_USAGE_ERROR -> The user caused an error in the command line.

/// Constant to indicate an unexpected error in the tester.
#define EXIT_INTERNAL_ERROR 2
/// Constant to indicate a usage error.
#define EXIT_USAGE_ERROR 3


/// Description of a tester.
struct kyua_cli_tester {
    /// Lists the test cases in a test program.
    ///
    /// \param test_program Path to the test program for which to list the test
    ///     cases.  Should be absolute.
    /// \param run_params Execution parameters to configure the test process.
    ///
    /// \return An error if the listing fails; OK otherwise.
    kyua_error_t (*list_test_cases)(const char* test_program,
                                    const kyua_run_params_t* run_params);

    /// Runs a single test cases of a test program.
    ///
    /// \param test_program Path to the test program for which to list the test
    ///     cases.  Should be absolute.
    /// \param test_case Name of the test case to run.
    /// \param result_file Path to the file to which to write the result of the
    ///     test.  Should be absolute.
    /// \param user_variables Array of name=value pairs that describe the user
    ///     configuration variables for the test case.
    /// \param run_params Execution parameters to configure the test process.
    /// \param [out] success Set to true if the test case reported a valid exit
    ///     condition (like "passed" or "skipped"); false otherwise.  This is
    ///     only updated if the method returns OK.
    ///
    /// \return An error if the listing fails; OK otherwise.
    kyua_error_t (*run_test_case)(const char* test_program,
                                  const char* test_case,
                                  const char* result_file,
                                  const char* const user_variables[],
                                  const kyua_run_params_t* run_params,
                                  bool* success);
};


int kyua_cli_main(const int, char* const* const, const kyua_cli_tester_t*);


#endif  // !defined(KYUA_CLI_H)
