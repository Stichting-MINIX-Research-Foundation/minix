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

/// \file engine/test_result.hpp
/// Representation of test case results.

#if !defined(ENGINE_TEST_RESULT_HPP)
#define ENGINE_TEST_RESULT_HPP

#include <istream>
#include <ostream>
#include <string>

namespace engine {


/// Representation of a single test result.
///
/// A test result is a simple pair of (type, reason).  The type indicates the
/// semantics of the results, and the optional reason provides an extra
/// description of the result type.
///
/// In general, a 'passed' result will not have a reason attached, because a
/// successful test case does not deserve any kind of explanation.  We used to
/// special-case this with a very complex class hierarchy, but it proved to
/// result in an extremely-complex to maintain code base that provided no
/// benefits.  As a result, we allow any test type to carry a reason.
class test_result {
public:
    /// Type definitions for all possible test case results.
    enum result_type {
        broken,
        expected_failure,
        failed,
        passed,
        skipped,
    };

private:
    /// The type of the result.
    result_type _type;

    /// A description of the result; may be empty.
    std::string _reason;

public:
    test_result(const result_type, const std::string& = "");
    static test_result parse(std::istream&);

    result_type type(void) const;
    const std::string& reason(void) const;

    bool good(void) const;

    bool operator==(const test_result&) const;
    bool operator!=(const test_result&) const;
};


std::ostream& operator<<(std::ostream&, const test_result&);


}  // namespace engine


#endif  // !defined(ENGINE_TEST_RESULT_HPP)
