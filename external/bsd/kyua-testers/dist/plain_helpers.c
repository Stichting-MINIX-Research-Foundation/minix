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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"


/// Exit code to report internal, unexpected errors.
static const int EXIT_BOGUS = 123;


/// Test case that always fails.
static int
fail_helper(void)
{
    fprintf(stdout, "First line to stdout\n");
    fprintf(stderr, "First line to stderr\n");
    return 78;
}


/// Test case that always passes.
static int
pass_helper(void)
{
    fprintf(stdout, "First line to stdout\n");
    fprintf(stdout, "Second line to stdout\n");
    fprintf(stderr, "First line to stderr\n");
    fprintf(stderr, "Second line to stderr\n");
    return EXIT_SUCCESS;
}


/// Test case that always dies due to a signal and dumps core.
static int
signal_helper(void)
{
    fprintf(stderr, "About to die due to SIGABRT!\n");
    abort();
}


/// Test case that sleeps for a long time.
static int
sleep_helper(void)
{
    sleep(300);
    return EXIT_FAILURE;
}


/// Dispatcher for individual testers based on the HELPER environment variable.
///
/// \param argc Number of arguments to the command.
/// \param unused_argv Arguments to the command.
///
/// \return An exit code.
int
main(const int argc, char* const* const KYUA_DEFS_UNUSED_PARAM(argv))
{
    if (argc != 1)
        errx(EXIT_BOGUS, "No arguments allowed");

    const char* helper_name = getenv("HELPER");
    if (helper_name == NULL) {
        errx(EXIT_BOGUS, "Must set HELPER to the name of the desired helper");
    }

    if (strcmp(helper_name, "fail") == 0) {
        return fail_helper();
    } else if (strcmp(helper_name, "pass") == 0) {
        return pass_helper();
    } else if (strcmp(helper_name, "signal") == 0) {
        return signal_helper();
    } else if (strcmp(helper_name, "sleep") == 0) {
        return sleep_helper();
    } else {
        errx(EXIT_BOGUS, "Unknown helper '%s'", helper_name);
    }
}
