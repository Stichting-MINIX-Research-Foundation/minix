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

#include "engine/test_case.hpp"

extern "C" {
#include <signal.h>
}

#include <fstream>

#include "engine/config.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "engine/testers.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Generates the set of configuration variables for the tester.
///
/// \param metadata The metadata of the test.
/// \param user_config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
///
/// \return The mapping of configuration variables for the tester.
static config::properties_map
generate_tester_config(const engine::metadata& metadata,
                       const config::tree& user_config,
                       const std::string& test_suite)
{
    config::properties_map props;

    try {
        props = user_config.all_properties(F("test_suites.%s") % test_suite,
                                           true);
    } catch (const config::unknown_key_error& unused_error) {
        // Ignore: not all test suites have entries in the configuration.
    }

    if (user_config.is_set("unprivileged_user")) {
        const passwd::user& user =
            user_config.lookup< engine::user_node >("unprivileged_user");
        props["unprivileged-user"] = user.name;
    }

    // TODO(jmmv): This is an ugly hack to cope with an atf-specific
    // property.  We should not be doing this at all, so just consider this
    // a temporary optimization...
    if (metadata.has_cleanup())
        props["has.cleanup"] = "true";
    else
        props["has.cleanup"] = "false";

    return props;
}


/// Creates a tester.
///
/// \param interface_name The name of the tester interface to use.
/// \param metadata Metadata of the test case.
/// \param user_config User-provided configuration variables.
///
/// \return The created tester, on which the test() method can be executed.
static engine::tester
create_tester(const std::string& interface_name,
              const engine::metadata& metadata, const config::tree& user_config)
{
    optional< passwd::user > user;
    if (user_config.is_set("unprivileged_user") &&
        metadata.required_user() == "unprivileged")
        user = user_config.lookup< engine::user_node >("unprivileged_user");

    return engine::tester(interface_name, user,
                          utils::make_optional(metadata.timeout()));
}


}  // anonymous namespace


/// Destructor.
engine::test_case_hooks::~test_case_hooks(void)
{
}


/// Called once the test case's stdout is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stdout.
void
engine::test_case_hooks::got_stdout(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Called once the test case's stderr is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stderr.
void
engine::test_case_hooks::got_stderr(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Internal implementation for a test_case.
struct engine::test_case::impl {
    /// Name of the interface implemented by the test program.
    const std::string interface_name;

    /// Test program this test case belongs to.
    const test_program& _test_program;

    /// Name of the test case; must be unique within the test program.
    std::string name;

    /// Test case metadata.
    metadata md;

    /// Fake result to return instead of running the test case.
    optional< test_result > fake_result;

    /// Constructor.
    ///
    /// \param interface_name_ Name of the interface implemented by the test
    ///     program.
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    /// \param md_ Metadata of the test case.
    /// \param fake_result_ Fake result to return instead of running the test
    ///     case.
    impl(const std::string& interface_name_,
         const test_program& test_program_,
         const std::string& name_,
         const metadata& md_,
         const optional< test_result >& fake_result_) :
        interface_name(interface_name_),
        _test_program(test_program_),
        name(name_),
        md(md_),
        fake_result(fake_result_)
    {
    }

    /// Equality comparator.
    ///
    /// \param other The other object to compare this one to.
    ///
    /// \return True if this object and other are equal; false otherwise.
    bool
    operator==(const impl& other) const
    {
        return (interface_name == other.interface_name &&
                (_test_program.absolute_path() ==
                 other._test_program.absolute_path()) &&
                name == other.name &&
                md == other.md &&
                fake_result == other.fake_result);
    }
};


/// Constructs a new test case.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.  This is a
///     static reference (instead of a test_program_ptr) because the test
///     program must exist in order for the test case to exist.
/// \param name_ The name of the test case within the test program.  Must be
///     unique.
/// \param md_ Metadata of the test case.
engine::test_case::test_case(const std::string& interface_name_,
                             const test_program& test_program_,
                             const std::string& name_,
                             const metadata& md_) :
    _pimpl(new impl(interface_name_, test_program_, name_, md_, none))
{
}



/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.
///
/// This is necessary for the cases where listing the test cases of a test
/// program fails.  In this scenario, we generate a single test case within
/// the test program that unconditionally returns a failure.
///
/// TODO(jmmv): Need to get rid of this.  We should be able to report the
/// status of test programs independently of test cases, as some interfaces
/// don't know about the latter at all.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
engine::test_case::test_case(
    const std::string& interface_name_,
    const test_program& test_program_,
    const std::string& name_,
    const std::string& description_,
    const engine::test_result& test_result_) :
    _pimpl(new impl(interface_name_, test_program_, name_,
                    metadata_builder().set_description(description_).build(),
                    utils::make_optional(test_result_)))
{
    PRE_MSG(name_.length() > 4 && name_.substr(0, 2) == "__" &&
            name_.substr(name_.length() - 2) == "__",
            "Invalid fake name provided to fake test case");
}


/// Destroys a test case.
engine::test_case::~test_case(void)
{
}


/// Gets the name of the interface implemented by the test program.
///
/// \return An interface name.
const std::string&
engine::test_case::interface_name(void) const
{
    return _pimpl->interface_name;
}


/// Gets the test program this test case belongs to.
///
/// \return A reference to the container test program.
const engine::test_program&
engine::test_case::container_test_program(void) const
{
    return _pimpl->_test_program;
}


/// Gets the test case name.
///
/// \return The test case name, relative to the test program.
const std::string&
engine::test_case::name(void) const
{
    return _pimpl->name;
}


/// Gets the test case metadata.
///
/// \return The test case metadata.
const engine::metadata&
engine::test_case::get_metadata(void) const
{
    return _pimpl->md;
}


/// Gets the fake result pre-stored for this test case.
///
/// \return A fake result, or none if not defined.
optional< engine::test_result >
engine::test_case::fake_result(void) const
{
    return _pimpl->fake_result;
}


/// Equality comparator.
///
/// \warning Because test cases reference their container test programs, and
/// test programs include test cases, we cannot perform a full comparison here:
/// otherwise, we'd enter an inifinte loop.  Therefore, out of necessity, this
/// does NOT compare whether the container test programs of the affected test
/// cases are the same.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
engine::test_case::operator==(const test_case& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
engine::test_case::operator!=(const test_case& other) const
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
engine::operator<<(std::ostream& output, const test_case& object)
{
    // We skip injecting container_test_program() on purpose to avoid a loop.
    output << F("test_case{interface=%s, name=%s, metadata=%s}")
        % text::quote(object.interface_name(), '\'')
        % text::quote(object.name(), '\'')
        % object.get_metadata();
    return output;
}


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
/// \param test_case The test case to debug.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param work_directory A directory that can be used to place temporary files.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stdout' is probably a reasonable value.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stderr' is probably a reasonable value.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::debug_test_case(const test_case* test_case,
                        const config::tree& user_config,
                        test_case_hooks& hooks,
                        const fs::path& work_directory,
                        const fs::path& stdout_path,
                        const fs::path& stderr_path)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return test_result(test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return test_result(test_result::broken, "Test program does not exist");

    const fs::auto_file result_file(work_directory / "result.txt");

    const engine::test_program& test_program =
        test_case->container_test_program();

    try {
        const engine::tester tester = create_tester(
            test_program.interface_name(), test_case->get_metadata(),
            user_config);
        tester.test(test_program.absolute_path(), test_case->name(),
                    result_file.file(), stdout_path, stderr_path,
                    generate_tester_config(test_case->get_metadata(),
                                           user_config,
                                           test_program.test_suite_name()));

        hooks.got_stdout(stdout_path);
        hooks.got_stderr(stderr_path);

        std::ifstream result_input(result_file.file().c_str());
        return engine::test_result::parse(result_input);
    } catch (const std::runtime_error& e) {
        // One of the possible explanation for us getting here is if the tester
        // crashes or doesn't behave as expected.  We must record any output
        // from the process so that we can debug it further.
        hooks.got_stdout(stdout_path);
        hooks.got_stderr(stderr_path);

        return engine::test_result(
            engine::test_result::broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}


/// Runs the test case.
///
/// \param test_case The test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param work_directory A directory that can be used to place temporary files.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::run_test_case(const test_case* test_case,
                      const config::tree& user_config,
                      test_case_hooks& hooks,
                      const fs::path& work_directory)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return test_result(test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return test_result(test_result::broken, "Test program does not exist");

    const fs::auto_file stdout_file(work_directory / "stdout.txt");
    const fs::auto_file stderr_file(work_directory / "stderr.txt");
    const fs::auto_file result_file(work_directory / "result.txt");

    const engine::test_program& test_program =
        test_case->container_test_program();

    try {
        const engine::tester tester = create_tester(
            test_program.interface_name(), test_case->get_metadata(),
            user_config);
        tester.test(test_program.absolute_path(), test_case->name(),
                    result_file.file(), stdout_file.file(), stderr_file.file(),
                    generate_tester_config(test_case->get_metadata(),
                                           user_config,
                                           test_program.test_suite_name()));

        hooks.got_stdout(stdout_file.file());
        hooks.got_stderr(stderr_file.file());

        std::ifstream result_input(result_file.file().c_str());
        return engine::test_result::parse(result_input);
    } catch (const std::runtime_error& e) {
        // One of the possible explanation for us getting here is if the tester
        // crashes or doesn't behave as expected.  We must record any output
        // from the process so that we can debug it further.
        hooks.got_stdout(stdout_file.file());
        hooks.got_stderr(stderr_file.file());

        return engine::test_result(
            engine::test_result::broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}
