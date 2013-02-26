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

#include "atf_list.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"


/// Expected header in the test program list.
#define TP_LIST_HEADER "Content-Type: application/X-atf-tp; version=\"1\""


/// Same as fgets, but removes any trailing newline from the output string.
///
/// \param [out] str Pointer to the output buffer.
/// \param size Length of the output buffer.
/// \param [in,out] stream File from which to read the line.
///
/// \return A pointer to the output buffer if successful; otherwise NULL.
static char*
fgets_no_newline(char* str, int size, FILE* stream)
{
    char* result = fgets(str, size, stream);
    if (result != NULL) {
        const size_t length = strlen(str);
        if (length > 0 && str[length - 1] == '\n')
            str[length - 1] = '\0';
    }
    return result;
}


/// Generates an error for the case where fgets() returns NULL.
///
/// \param input Stream on which fgets() returned an error.
/// \param message Error message.
///
/// \return An error object with the error message and any relevant details.
static kyua_error_t
fgets_error(FILE* input, const char* message)
{
    if (feof(input)) {
        return kyua_generic_error_new("%s: unexpected EOF", message);
    } else {
        assert(ferror(input));
        return kyua_libc_error_new(errno, "%s", message);
    }
}


/// Reads the header of the test cases list.
///
/// The header does not carry any useful information, so all this function does
/// is ensure the header is valid.
///
/// \param [in,out] input File from which to read the header.
///
/// \return OK if the header is valid; an error if it is not.
static kyua_error_t
parse_header(FILE* input)
{
    char line[80];  // It's ugly to have a limit, but it's easier this way.

    if (fgets_no_newline(line, sizeof(line), input) == NULL)
        return fgets_error(input, "fgets failed to read test cases list "
                           "header");
    if (strcmp(line, TP_LIST_HEADER) != 0)
        return kyua_generic_error_new("Invalid test cases list header '%s'",
                                      line);

    if (fgets_no_newline(line, sizeof(line), input) == NULL)
        return fgets_error(input, "fgets failed to read test cases list "
                           "header");
    if (strcmp(line, "") != 0)
        return kyua_generic_error_new("Incomplete test cases list header");

    return kyua_error_ok();
}


/// Looks for the first occurrence of any of the specified delimiters.
///
/// \param container String in which to look for the delimiters.
/// \param delimiters List of delimiters to look for.
///
/// \return A pointer to the first occurrence of the delimiter, or NULL if
/// there is none.
static char*
find_first_of(char* container, const char* delimiters)
{
    char* ptr = container;
    while (*ptr != '\0') {
        if (strchr(delimiters, *ptr) != NULL)
            return ptr;
        ++ptr;
    }
    return NULL;
}


/// Prints a string within single quotes, with proper escaping.
///
/// \param [in,out] line The line to be printed.  This is a non-const pointer
///     and the input string is modified to simplify tokenization.
/// \param [in,out] output Buffer onto which to write the quoted string.
/// \param surrounding If true, surround the printed value with single quotes.
static void
print_quoted(char* line, FILE* output, const bool surrounding)
{
    if (surrounding)
        fprintf(output, "'");

    char* quoteptr;
    while ((quoteptr = find_first_of(line, "\'\\")) != NULL) {
        const char quote = *quoteptr;
        *quoteptr = '\0';
        fprintf(output, "%s\\%c", line, quote);
        line = quoteptr + 1;
    }

    if (surrounding)
        fprintf(output, "%s'", line);
    else
        fprintf(output, "%s", line);
}


/// Parses a property from the test cases list.
///
/// The property is of the form "name: value", where the value extends to the
/// end of the line without quotations.
///
/// \param [in,out] line The line to be parsed.  This is a non-const pointer
///     and the input string is modified to simplify tokenization.
/// \param [out] key The name of the property if the parsing succeeds.  This
///     is a pointer within the input line.
/// \param [out] value The value of the property if the parsing succeeds.  This
///     is a pointer within the input line.
///
/// \return OK if the line contains a valid property; an error otherwise.
/// In case of success, both key and value are updated.
static kyua_error_t
parse_property(char* line, char** const key, char** const value)
{
    char* delim = strstr(line, ": ");
    if (delim == NULL)
        return kyua_generic_error_new("Invalid property '%s'", line);
    *delim = '\0'; *(delim + 1) = '\0';

    *key = line;
    *value = delim + 2;
    return kyua_error_ok();
}


/// Static value to denote an error in the return of rewrite_property;
static const char* rewrite_error = "ERROR";


/// Converts the name of an ATF property to a Kyua generic property.
///
/// \param name The name of the ATF property to process.
///
/// \return The name of the corresponding Kyua property if the input property is
/// valid; NULL if the property has a custom name that has to be handled in the
/// parent; or rewrite_error if the property is invalid.  If this returns
/// rewrite_error, it's OK to pointer-compare the return value to the static
/// symbol for equality.
static const char*
rewrite_property(const char* name)
{
    if (strcmp(name, "descr") == 0)
        return "description";
    else if (strcmp(name, "has.cleanup") == 0)
        return "has_cleanup";
    else if (strcmp(name, "require.arch") == 0)
        return "allowed_architectures";
    else if (strcmp(name, "require.config") == 0)
        return "required_configs";
    else if (strcmp(name, "require.files") == 0)
        return "required_files";
    else if (strcmp(name, "require.machine") == 0)
        return "allowed_platforms";
    else if (strcmp(name, "require.memory") == 0)
        return "required_memory";
    else if (strcmp(name, "require.progs") == 0)
        return "required_programs";
    else if (strcmp(name, "require.user") == 0)
        return "required_user";
    else if (strcmp(name, "timeout") == 0)
        return "timeout";
    else if (strlen(name) > 2 && name[0] == 'X' && name[1] == '-')
        return NULL;
    else
        return rewrite_error;
}


/// Parses a single test case and writes it to the output.
///
/// This has to be called after the ident property has been read, and takes care
/// of reading the rest of the test case and printing the parsed result.
///
/// Be aware that this consumes the newline after the test case.  The caller
/// should not look for it.
///
/// \param [in,out] input File from which to read the header.
/// \param [in,out] output File to which to write the parsed test case.
/// \param [in,out] name The name of the test case.  This is a non-const pointer
///     and the input string is modified to simplify tokenization.
///
/// \return OK if the parsing succeeds; an error otherwise.
static kyua_error_t
parse_test_case(FILE* input, FILE* output, char* name)
{
    kyua_error_t error;
    char line[1024];  // It's ugly to have a limit, but it's easier this way.

    fprintf(output, "test_case{name=");
    print_quoted(name, output, true);

    error = kyua_error_ok();
    while (!kyua_error_is_set(error) &&
           fgets_no_newline(line, sizeof(line), input) != NULL &&
           strcmp(line, "") != 0) {
        char* key; char* value;
        error = parse_property(line, &key, &value);
        if (!kyua_error_is_set(error)) {
            const char* out_key = rewrite_property(key);
            if (out_key == rewrite_error) {
                error = kyua_generic_error_new("Unknown ATF property %s", key);
            } else if (out_key == NULL) {
                fprintf(output, ", ['custom.");
                print_quoted(key, output, false);
                fprintf(output, "']=");
                print_quoted(value, output, true);
            } else {
                fprintf(output, ", %s=", out_key);
                print_quoted(value, output, true);
            }
        }
    }

    fprintf(output, "}\n");

    return error;
}


/// Rewrites the test cases list from the input to the output.
///
/// \param [in,out] input Stream from which to read the test program's test
///     cases list.  The current location must be after the header and at the
///     first identifier (if any).
/// \param [out] output Stream to which to write the generic list.
///
/// \return An error object.
static kyua_error_t
parse_tests(FILE* input, FILE* output)
{
    char line[512];  // It's ugly to have a limit, but it's easier this way.

    if (fgets_no_newline(line, sizeof(line), input) == NULL) {
        return fgets_error(input, "Empty test cases list");
    }

    kyua_error_t error;

    do {
        char* key; char* value;
        error = parse_property(line, &key, &value);
        if (kyua_error_is_set(error))
            break;

        if (strcmp(key, "ident") == 0) {
            error = parse_test_case(input, output, value);
        } else {
            error = kyua_generic_error_new("Expected ident property, got %s",
                                           key);
        }
    } while (!kyua_error_is_set(error) &&
             fgets_no_newline(line, sizeof(line), input) != NULL);

    if (!kyua_error_is_set(error)) {
        if (ferror(input))
            error = kyua_libc_error_new(errno, "fgets failed");
        else
            assert(feof(input));
    }

    return error;
}


/// Reads an ATF test cases list and prints a Kyua definition.
///
/// \param fd A file descriptor from which to read the test cases list of a test
///     program.  Should be connected to the stdout of the latter.  This
///     function grabs ownership of the descriptor and releases it in all cases.
/// \param [in,out] output File to which to write the Kyua definition.
///
/// \return OK if the parsing succeeds; an error otherwise.  Note that, if there
/// is an error, the output may not be consistent and should not be used.
kyua_error_t
atf_list_parse(const int fd, FILE* output)
{
    kyua_error_t error;

    FILE* input = fdopen(fd, "r");
    if (input == NULL) {
        error = kyua_libc_error_new(errno, "fdopen(%d) failed", fd);
        close(fd);
    } else {
        error = parse_header(input);
        if (!kyua_error_is_set(error)) {
            error = parse_tests(input, output);
        }
        fclose(input);
    }

    return error;
}
