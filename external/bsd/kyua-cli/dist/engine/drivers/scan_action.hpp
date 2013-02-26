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

/// \file engine/drivers/scan_action.hpp
/// Driver to scan the contents of an action.
///
/// This driver module implements the logic to scan the contents of an stored
/// action and to notify the presentation layer as soon as data becomes
/// available.  This is to prevent reading all the data from the action at once,
/// which could take too much memory.

#if !defined(ENGINE_DRIVERS_SCAN_ACTION_HPP)
#define ENGINE_DRIVERS_SCAN_ACTION_HPP

extern "C" {
#include <stdint.h>
}

#include "engine/test_program.hpp"
#include "store/transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"

namespace engine {

class action;
class test_result;

namespace drivers {
namespace scan_action {


/// Abstract definition of the hooks for this driver.
class base_hooks {
public:
    virtual ~base_hooks(void) = 0;

    /// Callback executed when an action is found.
    ///
    /// \param action_id The identifier of the loaded action.
    /// \param action The action loaded from the database.
    virtual void got_action(const int64_t action_id,
                            const engine::action& action) = 0;

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.  Some of the data are
    ///     lazily fetched, hence why we receive the object instead of the
    ///     individual elements.
    virtual void got_result(store::results_iterator& iter) = 0;
};


/// Tuple containing the results of this driver.
struct result {
    /// Initializer for the tuple's fields.
    result(void)
    {
    }
};


result drive(const utils::fs::path&, utils::optional< int64_t >,
             base_hooks&);


}  // namespace scan_action
}  // namespace drivers
}  // namespace engine

#endif  // !defined(ENGINE_DRIVERS_SCAN_ACTION_HPP)
