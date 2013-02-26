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

/// \file cli/common.hpp
/// Utility functions to implement CLI subcommands.

#if !defined(CLI_COMMON_HPP)
#define CLI_COMMON_HPP

#include <memory>
#include <set>

#include "utils/cmdline/base_command.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"

namespace utils {
namespace fs {
class path;
}  // namespace fs
}  // namespace utils

namespace engine {
struct test_filter;
class test_case;
class test_result;
}  // namespace engine

namespace cli {


extern const utils::cmdline::path_option build_root_option;
extern const utils::cmdline::path_option kyuafile_option;
extern const utils::cmdline::path_option store_option;
extern const utils::cmdline::property_option variable_option;


/// Base type for commands defined in the cli module.
///
/// All commands in Kyua receive a configuration object as their runtime
/// data parameter because the configuration file applies to all the
/// commands.
typedef utils::cmdline::base_command< utils::config::tree > cli_command;


/// Scoped, strictly owned pointer to a cli_command.
typedef std::auto_ptr< cli_command > cli_command_ptr;


utils::optional< utils::fs::path > get_home(void);

utils::optional< utils::fs::path > build_root_path(
    const utils::cmdline::parsed_cmdline&);
utils::fs::path kyuafile_path(const utils::cmdline::parsed_cmdline&);
utils::fs::path store_path(const utils::cmdline::parsed_cmdline&);

std::set< engine::test_filter > parse_filters(
    const utils::cmdline::args_vector&);
bool report_unused_filters(const std::set< engine::test_filter >&,
                           utils::cmdline::ui*);

std::string format_delta(const utils::datetime::delta&);
std::string format_result(const engine::test_result&);
std::string format_test_case_id(const engine::test_case&);
std::string format_test_case_id(const engine::test_filter&);


}  // namespace cli

#endif  // !defined(CLI_COMMON_HPP)
