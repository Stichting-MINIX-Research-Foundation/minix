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

/// \file engine/metadata.hpp
/// Representation of the metadata of a test program or test case.

#if !defined(ENGINE_METADATA_HPP)
#define ENGINE_METADATA_HPP

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include "utils/noncopyable.hpp"
#include "utils/shared_ptr.hpp"

namespace utils {
namespace config { class tree; }
namespace datetime { class delta; }
namespace fs { class path; }
namespace units { class bytes; }
}  // namespace utils

namespace engine {


// TODO(jmmv): All these types should probably be in individual header files so
// that we could include them without pulling in additional dependencies.
/// Collection of paths.
typedef std::set< utils::fs::path > paths_set;
/// Collection of strings.
typedef std::set< std::string > strings_set;
/// Collection of test properties in their textual form.
typedef std::map< std::string, std::string > properties_map;


extern utils::datetime::delta default_timeout;


class metadata_builder;


/// Collection of metadata properties of a test.
class metadata {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class metadata_builder;

public:
    metadata(const utils::config::tree&);
    ~metadata(void);

    const strings_set& allowed_architectures(void) const;
    const strings_set& allowed_platforms(void) const;
    properties_map custom(void) const;
    const std::string& description(void) const;
    bool has_cleanup(void) const;
    const strings_set& required_configs(void) const;
    const paths_set& required_files(void) const;
    const utils::units::bytes& required_memory(void) const;
    const paths_set& required_programs(void) const;
    const std::string& required_user(void) const;
    const utils::datetime::delta& timeout(void) const;

    engine::properties_map to_properties(void) const;

    bool operator==(const metadata&) const;
    bool operator!=(const metadata&) const;
};


std::ostream& operator<<(std::ostream&, const metadata&);


/// Builder for a metadata object.
class metadata_builder : utils::noncopyable {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::auto_ptr< impl > _pimpl;

public:
    metadata_builder(void);
    explicit metadata_builder(const engine::metadata&);
    ~metadata_builder(void);

    metadata_builder& add_allowed_architecture(const std::string&);
    metadata_builder& add_allowed_platform(const std::string&);
    metadata_builder& add_custom(const std::string&, const std::string&);
    metadata_builder& add_required_config(const std::string&);
    metadata_builder& add_required_file(const utils::fs::path&);
    metadata_builder& add_required_program(const utils::fs::path&);

    metadata_builder& set_allowed_architectures(const strings_set&);
    metadata_builder& set_allowed_platforms(const strings_set&);
    metadata_builder& set_custom(const properties_map&);
    metadata_builder& set_description(const std::string&);
    metadata_builder& set_has_cleanup(const bool);
    metadata_builder& set_required_configs(const strings_set&);
    metadata_builder& set_required_files(const paths_set&);
    metadata_builder& set_required_memory(const utils::units::bytes&);
    metadata_builder& set_required_programs(const paths_set&);
    metadata_builder& set_required_user(const std::string&);
    metadata_builder& set_string(const std::string&, const std::string&);
    metadata_builder& set_timeout(const utils::datetime::delta&);

    metadata build(void) const;
};


std::string check_reqs(const engine::metadata&, const utils::config::tree&,
                       const std::string&);


}  // namespace engine


#endif  // !defined(ENGINE_METADATA_HPP)
