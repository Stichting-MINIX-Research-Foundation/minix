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

/// \file error.h
/// High-level representation of error conditions.
///
/// The error module provides a mechanism to represent error conditions in an
/// efficient manner.  In the case of a successful operation, an error is
/// internally represented as a NULL pointer and thus has no overhead.  In the
/// case of an actual error, the representation is more complex and costly than
/// a traditional libc error number, but is also more verbose.  Because errors
/// are not (or should not be!) in the critical path, this is not a concern.

#if !defined(KYUA_ERROR_H)
#define KYUA_ERROR_H

#include "error_fwd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "defs.h"


/// Type of the per-error formatting function.
///
/// These functions take three arguments: the error to be formatted, a pointer
/// to the output buffer and the size of the output buffer.  The return value
/// indicates how many bytes were written to the output buffer, or a negative
/// value in case of an error.
typedef int (*kyua_error_format_callback)(
    struct kyua_error* const, char* const, const size_t);


/// Representation of an error.
struct kyua_error {
    /// Whether the error object has to be released or not.
    ///
    /// Sometimes (the oom error), a kyua_error_t object may point to a
    /// statically allocated error.  Such object cannot be freed.
    bool needs_free;

    /// Name of the type.
    const char* type_name;

    /// Opaquet error-specific data.
    void* data;

    /// Method to generate a textual representation of the error.
    kyua_error_format_callback format_callback;
};


kyua_error_t kyua_error_new(const char*, void*, size_t,
                            kyua_error_format_callback);
void kyua_error_free(kyua_error_t);
kyua_error_t kyua_error_subsume(kyua_error_t, kyua_error_t);

kyua_error_t kyua_error_ok(void);
bool kyua_error_is_set(const kyua_error_t);
bool kyua_error_is_type(const kyua_error_t, const char*);

const void* kyua_error_data(const kyua_error_t);
int kyua_error_format(const kyua_error_t, char* const, size_t);
void kyua_error_err(const int, const kyua_error_t, const char*, ...)
    KYUA_DEFS_NORETURN KYUA_DEFS_FORMAT_PRINTF(3, 4);
void kyua_error_fprintf(FILE*, const kyua_error_t, const char*, ...);
void kyua_error_warn(const kyua_error_t, const char*, ...);


extern const char* const kyua_generic_error_type;
kyua_error_t kyua_generic_error_new(const char* , ...)
    KYUA_DEFS_FORMAT_PRINTF(1, 2);


extern const char* const kyua_libc_error_type;
kyua_error_t kyua_libc_error_new(int, const char* , ...)
    KYUA_DEFS_FORMAT_PRINTF(2, 3);
int kyua_libc_error_errno(const kyua_error_t);


extern const char* const kyua_oom_error_type;
kyua_error_t kyua_oom_error_new(void);


extern const char* const kyua_usage_error_type;
kyua_error_t kyua_usage_error_new(const char* , ...)
    KYUA_DEFS_FORMAT_PRINTF(1, 2);


#endif  // !defined(KYUA_ERROR_H)
