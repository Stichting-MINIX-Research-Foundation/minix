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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "cli.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "error.h"
#include "run.h"

#if !defined(GID_MAX)
#   define GID_MAX INT_MAX
#endif
#if !defined(UID_MAX)
#   define UID_MAX INT_MAX
#endif

#if defined(HAVE_GETOPT_GNU)
#   define GETOPT_PLUS "+"
#else
#   define GETOPT_PLUS
#endif


/// Terminates execution if the given error is set.
///
/// \param error The error to validate.
///
/// \post The program terminates if the given error is set.
static void
check_error(kyua_error_t error)
{
    if (kyua_error_is_set(error)) {
        const bool usage_error = kyua_error_is_type(error,
                                                    kyua_usage_error_type);

        char buffer[1024];
        kyua_error_format(error, buffer, sizeof(buffer));
        kyua_error_free(error);

        errx(usage_error ? EXIT_USAGE_ERROR : EXIT_INTERNAL_ERROR,
             "%s", buffer);
    }
}


/// Converts a string to an unsigned long.
///
/// \param str The string to convert.
/// \param message Part of the error message to print if the string does not
///     represent a valid unsigned long number.
/// \param max Maximum accepted value.
///
/// \return The converted numerical value.
///
/// \post The program terminates if the value is invalid.
static unsigned long
parse_ulong(const char* str, const char* message, const unsigned long max)
{
    char *endptr;

    errno = 0;
    const unsigned long value = strtoul(str, &endptr, 10);
    if (str[0] == '\0' || *endptr != '\0')
        errx(EXIT_USAGE_ERROR, "%s '%s' (not a number)", message, str);
    else if (errno == ERANGE || value == LONG_MAX || value > max)
        errx(EXIT_USAGE_ERROR, "%s '%s' (out of range)", message, str);
    return value;
}


/// Clears getopt(3) state to allow calling the function again.
static void
reset_getopt(void)
{
    opterr = 0;
    optind = GETOPT_OPTIND_RESET_VALUE;
#if defined(HAVE_GETOPT_WITH_OPTRESET)
    optreset = 1;
#endif
}


/// Prints the list of test cases and their metadata in a test program.
///
/// \param argc Number of arguments to the command, including the command name.
/// \param argv Arguments to the command, including the command name.
/// \param tester Description of the tester implemented by this binary.
/// \param run_params Execution parameters to configure the test process.
///
/// \return An exit status to indicate the success or failure of the listing.
///
/// \post Usage errors terminate the execution of the program right away.
static int
list_command(const int argc, char* const* const argv,
             const kyua_cli_tester_t* tester,
             const kyua_run_params_t* run_params)
{
    if (argc < 2)
        errx(EXIT_USAGE_ERROR, "No test program provided");
    else if (argc > 2)
        errx(EXIT_USAGE_ERROR, "Only one test program allowed");
    const char* test_program = argv[1];

    check_error(tester->list_test_cases(test_program, run_params));
    return EXIT_SUCCESS;
}


/// Runs and cleans up a single test case.
///
/// \param argc Number of arguments to the command, including the command name.
/// \param argv Arguments to the command, including the command name.
/// \param tester Description of the tester implemented by this binary.
/// \param run_params Execution parameters to configure the test process.
///
/// \return An exit status to indicate the success or failure of the test case
/// execution.
///
/// \post Usage errors terminate the execution of the program right away.
static int
test_command(int argc, char* const* argv, const kyua_cli_tester_t* tester,
             const kyua_run_params_t* run_params)
{
#   define MAX_USER_VARIABLES 256
    const char* user_variables[MAX_USER_VARIABLES];

    const char** last_variable = user_variables;
    int ch;
    while ((ch = getopt(argc, argv, GETOPT_PLUS":v:")) != -1) {
        switch (ch) {
        case 'v':
            *last_variable++ = optarg;
            break;

        case ':':
            errx(EXIT_USAGE_ERROR, "%s's -%c requires an argument", argv[0],
                 optopt);

        case '?':
            errx(EXIT_USAGE_ERROR, "Unknown %s option -%c", argv[0], optopt);

        default:
            assert(false);
        }
    }
    argc -= optind;
    argv += optind;
    *last_variable = NULL;

    if (argc != 3)
        errx(EXIT_USAGE_ERROR, "Must provide a test program, a test case name "
             "and a result file");
    const char* test_program = argv[0];
    const char* test_case = argv[1];
    const char* result_file = argv[2];

    bool success;
    check_error(tester->run_test_case(test_program, test_case, result_file,
                                      user_variables, run_params, &success));
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}


/// Generic entry point to the tester's command-line interface.
///
/// \param argc Verbatim argc passed to main().
/// \param argv Verbatim argv passed to main().
/// \param tester Description of the tester implemented by this binary.
///
/// \return An exit status.
///
/// \post Usage errors terminate the execution of the program right away.
int
kyua_cli_main(int argc, char* const* argv, const kyua_cli_tester_t* tester)
{
    kyua_run_params_t run_params;
    kyua_run_params_init(&run_params);

    int ch;
    while ((ch = getopt(argc, argv, GETOPT_PLUS":g:t:u:")) != -1) {
        switch (ch) {
        case 'g':
            run_params.unprivileged_group = (uid_t)parse_ulong(
                optarg, "Invalid GID", GID_MAX);
            break;

        case 't':
            run_params.timeout_seconds = parse_ulong(
                optarg, "Invalid timeout value", LONG_MAX);
            break;

        case 'u':
            run_params.unprivileged_user = (uid_t)parse_ulong(
                optarg, "Invalid UID", UID_MAX);
            break;

        case ':':
            errx(EXIT_USAGE_ERROR, "-%c requires an argument", optopt);

        case '?':
            errx(EXIT_USAGE_ERROR, "Unknown option -%c", optopt);

        default:
            assert(false);
        }
    }
    argc -= optind;
    argv += optind;
    reset_getopt();

    if (argc == 0)
        errx(EXIT_USAGE_ERROR, "Must provide a command");
    const char* command = argv[0];

    // Keep sorted by order of likelyhood (yeah, micro-optimization).
    if (strcmp(command, "test") == 0)
        return test_command(argc, argv, tester, &run_params);
    else if (strcmp(command, "list") == 0)
        return list_command(argc, argv, tester, &run_params);
    else
        errx(EXIT_USAGE_ERROR, "Unknown command '%s'", command);
}
