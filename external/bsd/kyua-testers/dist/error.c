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

#include "error.h"

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/// Generic hook to format an error that does not have a format callback.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
static int
generic_format_callback(const kyua_error_t error, char* const output_buffer,
                        size_t output_size)
{
    assert(error != NULL);
    return snprintf(output_buffer, output_size, "Error '%s'", error->type_name);
}


/// Initializes an error object.
///
/// \param error Error for which to generate a message.
/// \param type_name Name of the error type.
/// \param data Opaque data that belongs to the error, for usage by
///     error-specific methods like format_callback.
/// \param data_size Size of the opaque data object.
/// \param format_callback Type-specific method to generate a user
///     representation of the error.
///
/// \return True if the initialization succeeds; false otherwise.  If
/// false, the error object passed in has not been modified.
static bool
error_init(kyua_error_t const error, const char* const type_name,
           void* const data, const size_t data_size,
           const kyua_error_format_callback format_callback)
{
    assert(data != NULL || data_size == 0);
    assert(data_size != 0 || data == NULL);

    bool ok;

    if (data == NULL) {
        error->data = NULL;
        error->needs_free = false;
        ok = true;
    } else {
        void* new_data = malloc(data_size);
        if (new_data == NULL) {
            ok = false;
        } else {
            memcpy(new_data, data, data_size);
            error->data = new_data;
            ok = true;
        }
    }

    if (ok) {
        error->type_name = type_name;
        error->format_callback = (format_callback == NULL) ?
            generic_format_callback : format_callback;
    }

    return ok;
}


/// Allocates and initializes a new error.
///
/// \param type_name Name of the error type.
/// \param data Opaque data that belongs to the error, for usage by
///     error-specific methods like format_callback.
/// \param data_size Size of the opaque data object.
/// \param format_callback Type-specific method to generate a user
///     representation of the error.
///
/// \return The newly initialized error, or an out of memory error.
kyua_error_t
kyua_error_new(const char* const type_name, void* const data,
               const size_t data_size,
               const kyua_error_format_callback format_callback)
{
    assert(data != NULL || data_size == 0);
    assert(data_size != 0 || data == NULL);

    kyua_error_t error = malloc(sizeof(struct kyua_error));
    if (error == NULL)
        error = kyua_oom_error_new();
    else {
        if (!error_init(error, type_name, data, data_size, format_callback)) {
            free(error);
            error = kyua_oom_error_new();
        } else {
            error->needs_free = true;
        }
    }

    assert(error != NULL);
    return error;
}


/// Releases an error.
///
/// \param error The error object to release.
void
kyua_error_free(kyua_error_t error)
{
    assert(error != NULL);

    const bool needs_free = error->needs_free;

    if (error->data != NULL)
        free(error->data);
    if (needs_free)
        free(error);
}


/// Returns the "most important" of two errors.
///
/// "Most important" is defined as: the primary error is returned if set,
/// otherwise the secondary error is returned.
///
/// It is the responsibility of the caller to free the *resulting* error of this
/// call.  The original errors passed in should not be consulted any longer,
/// because it is impossible to know which one was chosen.
///
/// \param primary The primary error to compare.
/// \param [in,out] secondary The secondary error to compare.  This is freed if
///     the primary error is set.
///
/// \return Either primary or secondary.
kyua_error_t
kyua_error_subsume(kyua_error_t primary, kyua_error_t secondary)
{
    if (kyua_error_is_set(primary)) {
        if (kyua_error_is_set(secondary))
            kyua_error_free(secondary);
        return primary;
    } else {
        return secondary;
    }
}


/// Constructor for a no-error condition.
///
/// \return Opaque representation of a no-error condition.
kyua_error_t
kyua_error_ok(void)
{
    return NULL;
}


/// Checks if the given error object represents an error or not.
///
/// \param error The error to check.
///
/// \return True if the error is set.
bool
kyua_error_is_set(const kyua_error_t error)
{
    return error != NULL;
}


/// Checks if the given error object is of a specific type.
///
/// \pre The error must be set.
///
/// \param error The error to check.
/// \param type_name The type of the expected error.
///
/// \return True if the error is of type type_name.
bool
kyua_error_is_type(const kyua_error_t error, const char* type_name)
{
    assert(error != NULL);

    return strcmp(error->type_name, type_name) == 0;
}


/// Returns a pointer to the error-specific data.
///
/// \pre The error must be set.
///
/// \param error The error to query.
///
/// \return An opaque pointer to the error data.  This should only be
/// dereferenced by the methods of the error class that created it.
const void*
kyua_error_data(const kyua_error_t error)
{
    assert(error != NULL);

    return error->data;
}


/// Generates a user-friendly representation of the error.
///
/// This cannot fail, but it is possible that the generated error does not
/// fit in the provided buffer.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
int
kyua_error_format(const kyua_error_t error, char* const output_buffer,
                  const size_t output_size)
{
    assert(kyua_error_is_set(error));
    return error->format_callback(error, output_buffer, output_size);
}


/// Formats a string and appends an error code to it.
///
/// \param error Error to append to the formatted message.
/// \param format User-specified message, as a formatting string.
/// \param ap List of arguments to the format string.
/// \param [out] output_buffer Buffer into which to write the message.
/// \param output_size Length of the output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
format_user_message(const kyua_error_t error, const char* format, va_list ap,
                    char* const output_buffer, const size_t output_size)
{
    assert(kyua_error_is_set(error));

    va_list ap2;
    va_copy(ap2, ap);
    size_t written = vsnprintf(output_buffer, output_size, format, ap2);
    va_end(ap2);
    if (written >= output_size)
        return -1;

    written += snprintf(output_buffer + written, output_size - written, ": ");
    if (written >= output_size)
        return -1;

    return kyua_error_format(error, output_buffer + written,
                             output_size - written);
}


/// Version of err(3) that works with kyua_error_t objects.
///
/// \param exit_code Error code with which to terminate the execution.
/// \param error Error to append to the output.
/// \param format User-specified message, as a formatting string.
/// \param ... Positional arguments to the format string.
///
/// \post Execution terminates with exit_code.
void
kyua_error_err(const int exit_code, const kyua_error_t error,
               const char* format, ...)
{
    char buffer[2048];

    va_list ap;
    va_start(ap, format);
    (void)format_user_message(error, format, ap, buffer, sizeof(buffer));
    va_end(ap);
    kyua_error_free(error);

    errx(exit_code, "%s", buffer);
}


/// Writes an error to a file stream.
///
/// \param stream Stream to which to write the message.
/// \param error Error to append to the output.  This is not released.
/// \param format User-specified message, as a formatting string.
/// \param ... Positional arguments to the format string.
void
kyua_error_fprintf(FILE* stream, const kyua_error_t error,
                   const char* format, ...)
{
    char buffer[2048];

    va_list ap;
    va_start(ap, format);
    (void)format_user_message(error, format, ap, buffer, sizeof(buffer));
    va_end(ap);

    fprintf(stream, "%s", buffer);
}


/// Version of warn(3) that works with kyua_error_t objects.
///
/// \param error Error to append to the output.  This is not released.
/// \param format User-specified message, as a formatting string.
/// \param ... Positional arguments to the format string.
void
kyua_error_warn(const kyua_error_t error, const char* format, ...)
{
    char buffer[2048];

    va_list ap;
    va_start(ap, format);
    (void)format_user_message(error, format, ap, buffer, sizeof(buffer));
    va_end(ap);

    warnx("%s", buffer);
}


/// Name of an generic error type.
const char* const kyua_generic_error_type = "generic";


/// Generates a user-friendly representation of the error.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
generic_format(const kyua_error_t error, char* const output_buffer,
             const size_t output_size)
{
    assert(kyua_error_is_type(error, kyua_generic_error_type));

    const char* message = kyua_error_data(error);
    return snprintf(output_buffer, output_size, "%s", message);
}


/// Constructs a new generic error.
///
/// \param message Textual description of the problem.
/// \param ... Positional arguments for the description.
///
/// \return The generated error.
kyua_error_t
kyua_generic_error_new(const char* message, ...)
{
    char formatted[1024];
    va_list ap;

    va_start(ap, message);
    (void)vsnprintf(formatted, sizeof(formatted), message, ap);
    va_end(ap);

    return kyua_error_new(kyua_generic_error_type, formatted, sizeof(formatted),
                          generic_format);
}


/// Name of a libc type.
const char* const kyua_libc_error_type = "libc";


/// Representation of a libc error.
struct libc_error_data {
    /// Value of the errno captured during the error creation.
    int original_errno;

    /// Explanation of the problem that lead to the error.
    char description[4096];
};
/// Shorthand for a libc_error_data structure.
typedef struct libc_error_data libc_error_data_t;


/// Generates a user-friendly representation of the error.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
libc_format(const kyua_error_t error, char* const output_buffer,
            const size_t output_size)
{
    assert(kyua_error_is_type(error, kyua_libc_error_type));

    const libc_error_data_t* data = kyua_error_data(error);
    return snprintf(output_buffer, output_size, "%s: %s", data->description,
                    strerror(data->original_errno));
}


/// Constructs a new libc error.
///
/// \param original_errno libc error code for this error.
/// \param description Textual description of the problem.
/// \param ... Positional arguments for the description.
///
/// \return The generated error.
kyua_error_t
kyua_libc_error_new(const int original_errno, const char* description, ...)
{
    va_list ap;

    const size_t data_size = sizeof(libc_error_data_t);
    libc_error_data_t* data = (libc_error_data_t*)malloc(data_size);
    if (data == NULL)
        return kyua_oom_error_new();

    data->original_errno = original_errno;
    va_start(ap, description);
    (void)vsnprintf(data->description, sizeof(data->description),
                    description, ap);
    va_end(ap);

    return kyua_error_new(kyua_libc_error_type, data, data_size, libc_format);
}


/// Extracts the original errno of a libc error.
///
/// \pre error must have been constructed by kyua_libc_error_new.
///
/// \param error The error object to access.
///
/// \return The libc error code.
int
kyua_libc_error_errno(const kyua_error_t error)
{
    assert(kyua_error_is_type(error, kyua_libc_error_type));

    const struct libc_error_data* data = kyua_error_data(error);
    return data->original_errno;
}


/// Name of an OOM type.
const char* const kyua_oom_error_type = "oom";


/// Data of an out of memory error.
///
/// All error types are allocated in dynamic memory.  However, doing so for
/// an out of memory error is not possible because, when we are out of
/// memory, we probably cannot allocate more memory to generate an error.
/// Therefore, we just keep a single static instance of the out of memory
/// error around all the time.
static struct kyua_error oom_error;


/// Generates a user-friendly representation of the error.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
oom_format(const kyua_error_t error, char* const output_buffer,
           const size_t output_size)
{
    assert(kyua_error_is_type(error, kyua_oom_error_type));

    return snprintf(output_buffer, output_size, "Not enough memory");
}


/// Constructs a new out-of-memory error.
///
/// This will always succeed because we just return a reference to the
/// statically-allocated oom_error.
///
/// \return An error representing an out of memory condition.
kyua_error_t
kyua_oom_error_new(void)
{
    // This is idempotent; no need to ensure that we call it only once.
    const bool ok = error_init(&oom_error, kyua_oom_error_type, NULL, 0,
                               oom_format);
    assert(ok);

    return &oom_error;
}


/// Name of an usage error type.
const char* const kyua_usage_error_type = "usage";


/// Generates a user-friendly representation of the error.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
usage_format(const kyua_error_t error, char* const output_buffer,
             const size_t output_size)
{
    assert(kyua_error_is_type(error, kyua_usage_error_type));

    const char* message = kyua_error_data(error);
    return snprintf(output_buffer, output_size, "%s", message);
}


/// Constructs a new usage error.
///
/// \param message Textual description of the problem.
/// \param ... Positional arguments for the description.
///
/// \return The generated error.
kyua_error_t
kyua_usage_error_new(const char* message, ...)
{
    char formatted[1024];
    va_list ap;

    va_start(ap, message);
    (void)vsnprintf(formatted, sizeof(formatted), message, ap);
    va_end(ap);

    return kyua_error_new(kyua_usage_error_type, formatted, sizeof(formatted),
                          usage_format);
}
