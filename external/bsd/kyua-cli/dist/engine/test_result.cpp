// Copyright 2010 Google Inc.
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

#include "engine/test_result.hpp"

#include "engine/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


/// Constructs a base result.
///
/// \param type_ The type of the result.
/// \param reason_ The reason explaining the result, if any.  It is OK for this
///     to be empty, which is actually the default.
engine::test_result::test_result(const result_type type_,
                                 const std::string& reason_) :
    _type(type_),
    _reason(reason_)
{
}


/// Parses a result from an input stream.
///
/// The parsing of a results file is quite permissive in terms of file syntax
/// validation.  We accept result files with or without trailing new lines, and
/// with descriptions that may span multiple lines.  This is to avoid getting in
/// trouble when the result is generated from user code, in which case it is
/// hard to predict how newlines look like.  Just swallow them; it's better for
/// the consumer.
///
/// \param input The stream from which to read the result.
///
/// \return The parsed result.  If there is any problem during parsing, the
/// failure is encoded as a broken result.
engine::test_result
engine::test_result::parse(std::istream& input)
{
    std::string line;
    if (!std::getline(input, line).good() && line.empty())
        return test_result(broken, "Empty result file");

    // Fast-path for the most common case.
    if (line == "passed")
        return test_result(passed);

    std::string type, reason;
    const std::string::size_type pos = line.find(": ");
    if (pos == std::string::npos) {
        type = line;
        reason = "";
    } else {
        type = line.substr(0, pos);
        reason = line.substr(pos + 2);
    }

    if (input.good()) {
        line.clear();
        while (std::getline(input, line).good() && !line.empty()) {
            reason += "<<NEWLINE>>" + line;
            line.clear();
        }
        if (!line.empty())
            reason += "<<NEWLINE>>" + line;
    }

    if (type == "broken") {
        return test_result(broken, reason);
    } else if (type == "expected_failure") {
        return test_result(expected_failure, reason);
    } else if (type == "failed") {
        return test_result(failed, reason);
    } else if (type == "passed") {
        return test_result(passed, reason);
    } else if (type == "skipped") {
        return test_result(skipped, reason);
    } else {
        return test_result(broken, F("Unknown result type '%s'") % type);
    }
}


/// Returns the type of the result.
///
/// \return A result type.
engine::test_result::result_type
engine::test_result::type(void) const
{
    return _type;
}


/// Returns the reason explaining the result.
///
/// \return A textual reason, possibly empty.
const std::string&
engine::test_result::reason(void) const
{
    return _reason;
}


/// True if the test case result has a positive connotation.
///
/// \return Whether the test case is good or not.
bool
engine::test_result::good(void) const
{
    switch (_type) {
    case expected_failure:
    case passed:
    case skipped:
        return true;

    case broken:
    case failed:
        return false;
    }
    UNREACHABLE;
}


/// Equality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is equal to this one, false otherwise.
bool
engine::test_result::operator==(const test_result& other) const
{
    return _type == other._type && _reason == other._reason;
}


/// Inequality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is different from this one, false
/// otherwise.
bool
engine::test_result::operator!=(const test_result& other) const
{
    return !(*this == other);
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
engine::operator<<(std::ostream& output, const test_result& object)
{
    std::string result_name;
    switch (object.type()) {
    case test_result::broken: result_name = "broken"; break;
    case test_result::expected_failure: result_name = "expected_failure"; break;
    case test_result::failed: result_name = "failed"; break;
    case test_result::passed: result_name = "passed"; break;
    case test_result::skipped: result_name = "skipped"; break;
    }
    const std::string& reason = object.reason();
    if (reason.empty()) {
        output << F("test_result{type=%s}") % text::quote(result_name, '\'');
    } else {
        output << F("test_result{type=%s, reason=%s}")
            % text::quote(result_name, '\'') % text::quote(reason, '\'');
    }
    return output;
}
