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

#include "cli/cmd_report_html.hpp"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>

#include "cli/common.ipp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/drivers/scan_action.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/text/templates.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;
namespace text = utils::text;

using utils::optional;


namespace {


/// Creates the report's top directory and fails if it exists.
///
/// \param directory The directory to create.
/// \param force Whether to wipe an existing directory or not.
///
/// \throw std::runtime_error If the directory already exists; this is a user
///     error that the user must correct.
/// \throw fs::error If the directory creation fails for any other reason.
static void
create_top_directory(const fs::path& directory, const bool force)
{
    if (force) {
        if (fs::exists(directory))
            fs::rm_r(directory);
    }

    try {
        fs::mkdir(directory, 0755);
    } catch (const fs::system_error& e) {
        if (e.original_errno() == EEXIST)
            throw std::runtime_error(F("Output directory '%s' already exists; "
                                       "maybe use --force?") %
                                     directory);
        else
            throw e;
    }
}


/// Generates a flat unique filename for a given test case.
///
/// \param test_case The test case for which to genereate the name.
///
/// \return A filename unique within a directory with a trailing HTML extension.
static std::string
test_case_filename(const engine::test_case& test_case)
{
    static const char* special_characters = "/:";

    std::string name = cli::format_test_case_id(test_case);
    std::string::size_type pos = name.find_first_of(special_characters);
    while (pos != std::string::npos) {
        name.replace(pos, 1, "_");
        pos = name.find_first_of(special_characters, pos + 1);
    }
    return name + ".html";
}


/// Adds a string to string map to the templates.
///
/// \param [in,out] templates The templates to add the map to.
/// \param props The map to add to the templates.
/// \param key_vector Name of the template vector that holds the keys.
/// \param value_vector Name of the template vector that holds the values.
static void
add_map(text::templates_def& templates, const config::properties_map& props,
        const std::string& key_vector, const std::string& value_vector)
{
    templates.add_vector(key_vector);
    templates.add_vector(value_vector);

    for (config::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        templates.add_to_vector(key_vector, (*iter).first);
        templates.add_to_vector(value_vector, (*iter).second);
    }
}


/// Generates an HTML report.
class html_hooks : public scan_action::base_hooks {
    /// User interface object where to report progress.
    cmdline::ui* _ui;

    /// The top directory in which to create the HTML files.
    fs::path _directory;

    /// Templates accumulator to generate the index.html file.
    text::templates_def _summary_templates;

    /// Generates a common set of templates for all of our files.
    ///
    /// \return A new templates object with common parameters.
    static text::templates_def
    common_templates(void)
    {
        text::templates_def templates;
        templates.add_variable("css", "report.css");
        return templates;
    }

    /// Adds a test case result to the summary.
    ///
    /// \param test_case The test case to be added.
    /// \param result The result of the test case.
    void
    add_to_summary(const engine::test_case& test_case,
                   const engine::test_result& result)
    {
        std::string test_cases_vector;
        std::string test_cases_file_vector;
        switch (result.type()) {
        case engine::test_result::broken:
            test_cases_vector = "broken_test_cases";
            test_cases_file_vector = "broken_test_cases_file";
            break;

        case engine::test_result::expected_failure:
            test_cases_vector = "xfail_test_cases";
            test_cases_file_vector = "xfail_test_cases_file";
            break;

        case engine::test_result::failed:
            test_cases_vector = "failed_test_cases";
            test_cases_file_vector = "failed_test_cases_file";
            break;

        case engine::test_result::passed:
            test_cases_vector = "passed_test_cases";
            test_cases_file_vector = "passed_test_cases_file";
            break;

        case engine::test_result::skipped:
            test_cases_vector = "skipped_test_cases";
            test_cases_file_vector = "skipped_test_cases_file";
            break;
        }
        INV(!test_cases_vector.empty());
        INV(!test_cases_file_vector.empty());

        _summary_templates.add_to_vector(test_cases_vector,
                                         cli::format_test_case_id(test_case));
        _summary_templates.add_to_vector(test_cases_file_vector,
                                         test_case_filename(test_case));
    }

    /// Instantiate a template to generate an HTML file in the output directory.
    ///
    /// \param templates The templates to use.
    /// \param template_name The name of the template.  This is automatically
    ///     searched for in the installed directory, so do not provide a path.
    /// \param output_name The name of the output file.  This is a basename to
    ///     be created within the output directory.
    ///
    /// \throw text::error If there is any problem applying the templates.
    void
    generate(const text::templates_def& templates,
             const std::string& template_name,
             const std::string& output_name) const
    {
        const fs::path miscdir(utils::getenv_with_default(
             "KYUA_MISCDIR", KYUA_MISCDIR));
        const fs::path template_file = miscdir / template_name;
        const fs::path output_path(_directory / output_name);

        _ui->out(F("Generating %s") % output_path);
        text::instantiate(templates, template_file, output_path);
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param ui_ User interface object where to report progress.
    /// \param directory_ The directory in which to create the HTML files.
    html_hooks(cmdline::ui* ui_, const fs::path& directory_) :
        _ui(ui_),
        _directory(directory_),
        _summary_templates(common_templates())
    {
        // Keep in sync with add_to_summary().
        _summary_templates.add_vector("broken_test_cases");
        _summary_templates.add_vector("broken_test_cases_file");
        _summary_templates.add_vector("xfail_test_cases");
        _summary_templates.add_vector("xfail_test_cases_file");
        _summary_templates.add_vector("failed_test_cases");
        _summary_templates.add_vector("failed_test_cases_file");
        _summary_templates.add_vector("passed_test_cases");
        _summary_templates.add_vector("passed_test_cases_file");
        _summary_templates.add_vector("skipped_test_cases");
        _summary_templates.add_vector("skipped_test_cases_file");
    }

    /// Callback executed when an action is found.
    ///
    /// \param action_id The identifier of the loaded action.
    /// \param action The action loaded from the database.
    void
    got_action(const int64_t action_id,
               const engine::action& action)
    {
        _summary_templates.add_variable("action_id", F("%s") % action_id);

        const engine::context& context = action.runtime_context();
        text::templates_def templates = common_templates();
        templates.add_variable("action_id", F("%s") % action_id);
        templates.add_variable("cwd", context.cwd().str());
        add_map(templates, context.env(), "env_var", "env_var_value");
        generate(templates, "context.html", "context.html");
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void
    got_result(store::results_iterator& iter)
    {
        const engine::test_program_ptr test_program = iter.test_program();
        const engine::test_result result = iter.result();

        const engine::test_case& test_case = *test_program->find(
            iter.test_case_name());

        add_to_summary(test_case, result);

        text::templates_def templates = common_templates();
        templates.add_variable("test_case",
                               cli::format_test_case_id(test_case));
        templates.add_variable("test_program",
                               test_program->absolute_path().str());
        templates.add_variable("result", cli::format_result(result));
        templates.add_variable("duration", cli::format_delta(iter.duration()));

        add_map(templates, test_case.get_metadata().to_properties(),
                "metadata_var", "metadata_value");

        {
            const std::string stdout_text = iter.stdout_contents();
            if (!stdout_text.empty())
                templates.add_variable("stdout", stdout_text);
        }
        {
            const std::string stderr_text = iter.stderr_contents();
            if (!stderr_text.empty())
                templates.add_variable("stderr", stderr_text);
        }

        generate(templates, "test_result.html", test_case_filename(test_case));
    }

    /// Writes the index.html file in the output directory.
    ///
    /// This should only be called once all the processing has been done;
    /// i.e. when the scan_action driver returns.
    void
    write_summary(void)
    {
        const std::size_t bad_count =
            _summary_templates.get_vector("broken_test_cases").size() +
            _summary_templates.get_vector("failed_test_cases").size();
        _summary_templates.add_variable("bad_tests_count", F("%s") % bad_count);

        generate(text::templates_def(), "report.css", "report.css");
        generate(_summary_templates, "index.html", "index.html");
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report_html.
cli::cmd_report_html::cmd_report_html(void) : cli_command(
    "report-html", "", 0, 0,
    "Generates an HTML report with the result of a previous action")
{
    add_option(store_option);
    add_option(cmdline::int_option(
        "action", "The action to report; if not specified, defaults to the "
        "latest action in the database", "id"));
    add_option(cmdline::bool_option(
        "force", "Wipe the output directory before generating the new report; "
        "use care"));
    add_option(cmdline::path_option(
        "output", "The directory in which to store the HTML files",
        "path", "html"));
}


/// Entry point for the "report-html" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_user_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cli::cmd_report_html::run(cmdline::ui* ui,
                          const cmdline::parsed_cmdline& cmdline,
                          const config::tree& UTILS_UNUSED_PARAM(user_config))
{
    optional< int64_t > action_id;
    if (cmdline.has_option("action"))
        action_id = cmdline.get_option< cmdline::int_option >("action");

    const fs::path directory =
        cmdline.get_option< cmdline::path_option >("output");
    create_top_directory(directory, cmdline.has_option("force"));
    html_hooks hooks(ui, directory);
    scan_action::drive(store_path(cmdline), action_id, hooks);
    hooks.write_summary();

    return EXIT_SUCCESS;
}
