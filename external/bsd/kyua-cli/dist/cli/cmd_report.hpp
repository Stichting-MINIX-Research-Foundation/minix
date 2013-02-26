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

/// \file cli/cmd_report.hpp
/// Provides the cmd_report class.

#if !defined(CLI_CMD_REPORT_HPP)
#define CLI_CMD_REPORT_HPP

#include <fstream>
#include <memory>

#include "cli/common.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"

namespace cli {


/// Wrapper class to send messages through the UI or to a file.
///
/// The cmdline::ui object provides methods to write messages to stdout and
/// stderr.  We are interested in using these methods when dumping a report to
/// any of these channels, because this provides us proper logging among other
/// goodies during testing.  However, these are unsuitable to write the output
/// to an arbitrary file, which is a necessity for reports.
///
/// Therefore, this class provides a mechanism to write stdout and stderr
/// messages through the cmdline::ui object if the user so wishes, but otherwise
/// prints messages to the user selected file.
class file_writer : utils::noncopyable {
    /// The UI object to write stdout and stderr messages through.
    utils::cmdline::ui* const _ui;

    /// The path to the output file.
    const utils::fs::path _output_path;

    /// The output file, if not stdout nor stderr.
    std::auto_ptr< std::ofstream > _output_file;

    /// Constant that represents the path to stdout.
    static const utils::fs::path _stdout_path;

    /// Constant that represents the path to stderr.
    static const utils::fs::path _stderr_path;

public:
    file_writer(utils::cmdline::ui* const, const utils::fs::path&);
    ~file_writer(void);

    void operator()(const std::string&);
};


/// Implementation of the "report" subcommand.
class cmd_report : public cli_command
{
public:
    cmd_report(void);

    int run(utils::cmdline::ui*, const utils::cmdline::parsed_cmdline&,
            const utils::config::tree&);
};


}  // namespace cli


#endif  // !defined(CLI_CMD_REPORT_HPP)
