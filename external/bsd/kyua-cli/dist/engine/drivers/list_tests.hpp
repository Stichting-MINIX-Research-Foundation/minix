// Copyright 2011 Google Inc.
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

/// \file engine/drivers/list_tests.hpp
/// Driver to obtain a list of test cases out of a test suite.
///
/// This driver module implements the logic to extract a list of test cases out
/// of a particular test suite.

#if !defined(ENGINE_DRIVERS_LIST_TESTS_HPP)
#define ENGINE_DRIVERS_LIST_TESTS_HPP

#include <set>

#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"

namespace engine {
namespace drivers {
namespace list_tests {


/// Abstract definition of the hooks for this driver.
class base_hooks {
public:
    virtual ~base_hooks(void) = 0;

    /// Called when a test case is identified in a test suite.
    ///
    /// \param test_case The data describing the test case.
    virtual void got_test_case(const engine::test_case& test_case) = 0;
};


/// Tuple containing the results of this driver.
class result {
public:
    /// Filters that did not match any available test case.
    ///
    /// The presence of any filters here probably indicates a usage error.  If a
    /// test filter does not match any test case, it is probably a typo.
    std::set< test_filter > unused_filters;

    /// Initializer for the tuple's fields.
    ///
    /// \param unused_filters_ The filters that did not match any test case.
    result(const std::set< test_filter >& unused_filters_) :
        unused_filters(unused_filters_)
    {
    }
};


result drive(const utils::fs::path&, const utils::optional< utils::fs::path >,
             const std::set< test_filter >&, base_hooks&);


}  // namespace list_tests
}  // namespace drivers
}  // namespace engine

#endif  // !defined(ENGINE_DRIVERS_LIST_TESTS_HPP)
