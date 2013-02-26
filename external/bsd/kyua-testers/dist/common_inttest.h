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

/// \file common_inttest.h
/// Common integration tests for the tester binaries.

#if defined(KYUA_COMMON_INTTEST_H)
#   error "common_inttest.h can only be defined once"
#endif
/// Include guard.
#define KYUA_COMMON_INTTEST_H

#if !defined(INTERFACE)
#   error "Must define INTERFACE to the name of the tester interface"
#endif

#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#include "cli.h"  // For the EXIT_* constants only.
#include "defs.h"


/// Path to the installed testers.
static const char* default_testersdir = TESTERSDIR;


/// Returns the name of the current tester.
#define TESTER_BIN "kyua-" INTERFACE "-tester"


/// Returns the path to the helpers.
///
/// \param tc Pointer to the caller test case, to obtain the srcdir property.
///
/// \return A dynamically-allocated string; must be released with free(3).
static char*
helpers_path(const atf_tc_t* tc)
{
    const char* srcdir = atf_tc_get_config_var(tc, "srcdir");
    const char* name = INTERFACE "_helpers";

    const size_t length = strlen(srcdir) + 1 + strlen(name) + 1;
    char* buffer = (char*)malloc(length);
    (void)snprintf(buffer, length, "%s/%s", srcdir, name);
    return buffer;
}


/// Returns the path to the tester.
///
/// \return A dynamically-allocated string; must be released with free(3).
static char*
tester_path(void)
{
    const char* testersdir = getenv("TESTERSDIR");
    if (testersdir == NULL)
        testersdir = default_testersdir;
    const char* name = TESTER_BIN;

    const size_t length = strlen(testersdir) + 1 + strlen(name) + 1;
    char* buffer = (char*)malloc(length);
    ATF_REQUIRE(buffer != NULL);
    (void)snprintf(buffer, length, "%s/%s", testersdir, name);
    return buffer;
}


/// Initializes the test case metadata and the helpers.
///
/// \param [in,out] tc The test case in which to set the property.
/// \param uses_helpers Whether the test uses the helpers or not.
static void
setup(atf_tc_t* tc, const bool uses_helpers)
{
    char* tester = tester_path();
    if (uses_helpers) {
        char* helpers = helpers_path(tc);
        atf_tc_set_md_var(tc, "require.progs", "%s %s", tester, helpers);
        free(helpers);
    } else {
        atf_tc_set_md_var(tc, "require.progs", "%s", tester);
    }
    free(tester);
}


static void execute(va_list ap) KYUA_DEFS_NORETURN;


/// Executes the tester with the given set of variable arguments.
///
/// \param ap List of arguments to the tester.
static void
execute(va_list ap)
{
    const char* args[16];

    const char** current_arg = &args[0];
    *current_arg = TESTER_BIN;
    ++current_arg;
    while ((*current_arg = va_arg(ap, const char*)) != NULL)
        ++current_arg;

    char* program = tester_path();
    (void)execv(program, KYUA_DEFS_UNCONST(args));
    free(program);
    err(111, "Failed to execute %s", program);
}


/// Executes the tester and validates its output.
///
/// \param expected_exit_status Expected exit status of the subprocess.
/// \param expected_stdout Expected contents of stdout.
/// \param expected_stderr Expected contents of stderr.
/// \param ... Arguments to the tester, not including the program name.
static void
check(const int expected_exit_status, const char* expected_stdout,
      const char* expected_stderr, ...)
{
    const pid_t pid = atf_utils_fork();
    if (pid == 0) {
        va_list ap;
        va_start(ap, expected_stderr);
        execute(ap);
        va_end(ap);
    } else {
        atf_utils_wait(pid, expected_exit_status, expected_stdout,
                       expected_stderr);
    }
}


ATF_TC(top__missing_command);
ATF_TC_HEAD(top__missing_command, tc) { setup(tc, false); }
ATF_TC_BODY(top__missing_command, tc)
{
    check(EXIT_USAGE_ERROR, "", TESTER_BIN": Must provide a command\n",
          NULL);
}


ATF_TC(top__unknown_command);
ATF_TC_HEAD(top__unknown_command, tc) { setup(tc, false); }
ATF_TC_BODY(top__unknown_command, tc)
{
    check(EXIT_USAGE_ERROR, "", TESTER_BIN": Unknown command 'foo'\n",
          "foo", NULL);
}
