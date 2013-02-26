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

#include "text.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "error.h"


/// Calculates the length of a formatting string with its replacements.
///
/// \param format The formatting string.
/// \param ap List of arguments to apply to the formatting string.
///
/// \return -1 if there is an error; or the final length otherwise.
static int
calculate_length(const char* format, va_list ap)
{
    va_list ap2;

    char buffer[1];
    va_copy(ap2, ap);
    const int needed = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap2);
    return needed;
}


/// Generates a string from a format string and its replacements.
///
/// \param [out] output Pointer to the dynamically-allocated string with the
///     result of the operation.  The caller must release this with free().
/// \param format The formatting string.
/// \param ... Arguments to apply to the formatting string.
///
/// \return OK if the string could be formatted; an error otherwise.
kyua_error_t
kyua_text_printf(char** output, const char* format, ...)
{
    kyua_error_t error;
    va_list ap;

    va_start(ap, format);
    error = kyua_text_vprintf(output, format, ap);
    va_end(ap);

    return error;
}


/// Generates a string from a format string and its replacements.
///
/// \param [out] output Pointer to the dynamically-allocated string with the
///     result of the operation.  The caller must release this with free().
/// \param format The formatting string.
/// \param ap List of to apply to the formatting string.
///
/// \return OK if the string could be formatted; an error otherwise.
kyua_error_t
kyua_text_vprintf(char** output, const char* format, va_list ap)
{
    va_list ap2;

    va_copy(ap2, ap);
    const int length = calculate_length(format, ap2);
    va_end(ap2);
    if (length < 0)
        return kyua_libc_error_new(errno, "Could not calculate length of "
                                   "string with format '%s'", format);

    char* buffer = (char*)malloc(length + 1);
    if (buffer == NULL)
        return kyua_oom_error_new();

    va_copy(ap2, ap);
    const int printed_length = vsnprintf(buffer, length + 1, format, ap2);
    va_end(ap2);
    assert(printed_length == length);
    if (printed_length < 0) {
        free(buffer);
        return kyua_libc_error_new(errno, "Could generate string with format "
                                   "'%s'", format);
    }

    *output = buffer;
    return kyua_error_ok();
}
