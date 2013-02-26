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

/// \file engine/testers.hpp
/// Invocation of external tester binaries.

#if !defined(ENGINE_TESTERS_HPP)
#define ENGINE_TESTERS_HPP

#include <map>
#include <string>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/optional.hpp"
#include "utils/passwd.hpp"

namespace utils {
namespace config {
class tree;
}  // namespace config
namespace fs {
class path;
}  // namespace fs
}  // namespace utils

namespace engine {


/// Abstraction to invoke an external tester.
///
/// This class provides the primitives to construct an invocation of an external
/// tester.  In other words: this is the place where the knowledge of what
/// arguments a tester receives and the output it returns.
class tester {
    /// Name of the tester interface to use.
    std::string _interface;

    /// Common arguments to the tester, to be passed before the subcommand.
    std::vector< std::string > _common_args;

public:
    tester(const std::string&, const utils::optional< utils::passwd::user >&,
           const utils::optional< utils::datetime::delta >&);
    ~tester(void);

    std::string list(const utils::fs::path&) const;
    void test(const utils::fs::path&, const std::string&,
              const utils::fs::path&, const utils::fs::path&,
              const utils::fs::path&,
              const std::map< std::string, std::string >&) const;
};


utils::fs::path tester_path(const std::string&);


}  // namespace engine

#endif  // !defined(ENGINE_TESTERS_HPP)
