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

#include "result.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "error.h"


/// Mapping of kyua_result_type_t values to their textual representation.
static const char* type_to_name[] = {
    "passed",  // KYUA_RESULT_PASSED
    "failed",  // KYUA_RESULT_FAILED
    "broken",  // KYUA_RESULT_BROKEN
    "skipped",  // KYUA_RESULT_SKIPPED
    "expected_failure",  // KYUA_RESULT_EXPECTED_FAILURE
};


/// Creates a file with the result of the test.
///
/// \param path Path to the file to be created.
/// \param type The type of the result.
/// \param reason Textual explanation of the reason behind the result.  Must be
///     NULL with KYUA_RESULT_PASSED, or else non-NULL.
///
/// \return An error object.
kyua_error_t
kyua_result_write(const char* path, const enum kyua_result_type_t type,
                  const char* reason, ...)
{
    assert(type == KYUA_RESULT_PASSED || reason != NULL);
    assert(reason == NULL || type != KYUA_RESULT_PASSED);

    FILE* file = fopen(path, "w");
    if (file == NULL)
        return kyua_libc_error_new(errno, "Cannot create result file '%s'",
                                   path);
    if (reason != NULL) {
        char buffer[1024];
        va_list ap;
        va_start(ap, reason);
        (void)vsnprintf(buffer, sizeof(buffer), reason, ap);
        va_end(ap);
        fprintf(file, "%s: %s\n", type_to_name[type], buffer);
    } else {
        fprintf(file, "%s\n", type_to_name[type]);
    }
    fclose(file);

    return kyua_error_ok();
}
