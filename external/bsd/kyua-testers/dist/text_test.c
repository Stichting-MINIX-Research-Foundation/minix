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

#include <stdarg.h>
#include <stdlib.h>

#include <atf-c.h>

#include "error.h"


static kyua_error_t call_vprintf(char** output, const char* format, ...)
    KYUA_DEFS_FORMAT_PRINTF(2, 3);


/// Invokes kyua_text_vprintf() based on a set of variable arguments.
///
/// \param [out] output Output pointer of kyua_text_vprintf().
/// \param format Formatting string to use.
/// \param ... Variable arguments to pack in a va_list to use.
///
/// \return The return value of the wrapped function.
static kyua_error_t
call_vprintf(char** output, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    kyua_error_t error = kyua_text_vprintf(output, format, ap);
    va_end(ap);
    return error;
}


ATF_TC_WITHOUT_HEAD(printf__empty);
ATF_TC_BODY(printf__empty, tc)
{
    char* buffer;
    kyua_error_t error = kyua_text_printf(&buffer, "%s", "");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("", buffer);
}


ATF_TC_WITHOUT_HEAD(printf__some);
ATF_TC_BODY(printf__some, tc)
{
    char* buffer;
    kyua_error_t error = kyua_text_printf(&buffer, "this is %d %s", 123, "foo");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("this is 123 foo", buffer);
    free(buffer);
}


ATF_TC_WITHOUT_HEAD(vprintf__empty);
ATF_TC_BODY(vprintf__empty, tc)
{
    char* buffer;
    kyua_error_t error = call_vprintf(&buffer, "%s", "");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("", buffer);
}


ATF_TC_WITHOUT_HEAD(vprintf__some);
ATF_TC_BODY(vprintf__some, tc)
{
    char* buffer;
    kyua_error_t error = call_vprintf(&buffer, "this is %d %s", 123, "foo");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("this is 123 foo", buffer);
    free(buffer);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, printf__empty);
    ATF_TP_ADD_TC(tp, printf__some);

    ATF_TP_ADD_TC(tp, vprintf__empty);
    ATF_TP_ADD_TC(tp, vprintf__some);

    return atf_no_error();
}
