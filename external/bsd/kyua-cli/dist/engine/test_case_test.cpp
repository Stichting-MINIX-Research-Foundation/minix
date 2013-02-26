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
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/exceptions.hpp"
#include "engine/kyuafile.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Test case hooks to capture stdout and stderr in memory.
class capture_hooks : public engine::test_case_hooks {
public:
    /// Contents of the stdout of the test case.
    std::string stdout_contents;

    /// Contents of the stderr of the test case.
    std::string stderr_contents;

    /// Stores the stdout of the test case into stdout_contents.
    ///
    /// \param file The path to the file containing the stdout.
    void
    got_stdout(const fs::path& file)
    {
        atf::utils::cat_file(file.str(), "helper stdout:");
        ATF_REQUIRE(stdout_contents.empty());

        std::ifstream input(file.c_str());
        ATF_REQUIRE(input);
        stdout_contents = utils::read_stream(input);
    }

    /// Stores the stderr of the test case into stderr_contents.
    ///
    /// \param file The path to the file containing the stderr.
    void
    got_stderr(const fs::path& file)
    {
        atf::utils::cat_file(file.str(), "helper stderr:");
        ATF_REQUIRE(stderr_contents.empty());

        std::ifstream input(file.c_str());
        ATF_REQUIRE(input);
        stderr_contents = utils::read_stream(input);
    }
};


/// Launcher for the helper test cases.
///
/// This builder class can be used to construct the runtime state of the helper
/// test cases and later run them.  The class also provides other helper methods
/// to interact with the helper binary.
class atf_helper : utils::noncopyable {
    /// Path to the test program's source directory.
    const fs::path _srcdir;

    /// The root of the test suite.
    fs::path _root;

    /// Path to the helper test program, relative to _root.
    fs::path _binary_path;

    /// Name of the helper test case to run.
    const std::string _name;

    /// Metadata of the test case.
    engine::metadata_builder _mdbuilder;

    /// Run-time configuration for the test case.
    config::tree _user_config;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    atf_helper(const atf::tests::tc* atf_tc, const char* name) :
        _srcdir(atf_tc->get_config_var("srcdir")),
        _root(_srcdir),
        _binary_path("test_case_atf_helpers"),
        _name(name),
        _user_config(engine::default_config())
    {
        _user_config.set_string("architecture", "mock-architecture");
        _user_config.set_string("platform", "mock-platform");
    }

    /// Provides raw access to the run-time configuration.
    ///
    /// To override test-suite-specific variables, use set_config() as it
    /// abstracts away the name of the fake test suite.
    ///
    /// \returns A reference to the test case configuration.
    config::tree&
    config(void)
    {
        return _user_config;
    }

    /// Sets a test-suite-specific configuration variable for the helper.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set_config(const char* variable, const T& value)
    {
        _user_config.set_string(F("test_suites.the-suite.%s") % variable,
                                F("%s") % value);
    }

    /// Sets a metadata variable for the helper.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set_metadata(const char* variable, const T& value)
    {
        _mdbuilder.set_string(variable, F("%s") % value);
    }

    /// Places the helper in a different location.
    ///
    /// This prepares the helper to be run from a different location than the
    /// source directory so that the runtime execution can be validated.
    ///
    /// \param new_binary_path The new path to the binary, relative to the test
    ///     suite root.
    /// \param new_root The new test suite root.
    ///
    /// \pre The directory holding the target test program must exist.
    ///     Otherwise, the relocation of the binary will fail.
    void
    move(const char* new_binary_path, const char* new_root)
    {
        _binary_path = fs::path(new_binary_path);
        _root = fs::path(new_root);

        const fs::path src_path = fs::path(_srcdir / "test_case_atf_helpers");
        const fs::path new_path = _root / _binary_path;
        ATF_REQUIRE(
            ::symlink(src_path.c_str(), new_path.c_str()) != -1);
    }

    /// Runs the helper.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(void) const
    {
        engine::test_case_hooks dummy_hooks;
        return run(dummy_hooks);
    }

    /// Runs the helper.
    ///
    /// \param hooks The hooks to pass to the test case.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(engine::test_case_hooks& hooks) const
    {
        const engine::test_program test_program(
            "atf", _binary_path, _root, "the-suite",
            engine::metadata_builder().build());
        const engine::test_case test_case("atf", test_program, _name,
                                          _mdbuilder.build());

        const fs::path workdir("work");
        fs::mkdir(workdir, 0755);

        const engine::test_result result = engine::run_test_case(
            &test_case, _user_config, hooks, workdir);
        ATF_REQUIRE(::rmdir(workdir.c_str()) != -1);
        return result;
    }
};


/// Hooks to retrieve stdout and stderr.
class fetch_output_hooks : public engine::test_case_hooks {
public:
    /// Copies the stdout of the test case outside of its work directory.
    ///
    /// \param file The location of the test case's stdout.
    void
    got_stdout(const fs::path& file)
    {
        atf::utils::copy_file(file.str(), "helper-stdout.txt");
        atf::utils::cat_file("helper-stdout.txt", "helper stdout: ");
    }

    /// Copies the stderr of the test case outside of its work directory.
    ///
    /// \param file The location of the test case's stderr.
    void
    got_stderr(const fs::path& file)
    {
        atf::utils::copy_file(file.str(), "helper-stderr.txt");
        atf::utils::cat_file("helper-stderr.txt", "helper stderr: ");
    }
};


/// Simplifies the execution of the helper test cases.
class plain_helper {
    /// Path to the test program's source directory.
    const fs::path _srcdir;

    /// The root of the test suite.
    fs::path _root;

    /// Path to the helper test program, relative to _root.
    fs::path _binary_path;

    /// Optional timeout for the test program.
    optional< datetime::delta > _timeout;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    /// \param timeout An optional timeout for the test case.
    plain_helper(const atf::tests::tc* atf_tc, const char* name,
                 const optional< datetime::delta > timeout = none) :
        _srcdir(atf_tc->get_config_var("srcdir")),
        _root(_srcdir),
        _binary_path("test_case_plain_helpers"),
        _timeout(timeout)
    {
        utils::setenv("TEST_CASE", name);
    }

    /// Sets an environment variable for the helper.
    ///
    /// This is simply syntactic sugar for utils::setenv.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set(const char* variable, const T& value)
    {
        utils::setenv(variable, F("%s") % value);
    }

    /// Places the helper in a different location.
    ///
    /// This prepares the helper to be run from a different location than the
    /// source directory so that the runtime execution can be validated.
    ///
    /// \param new_binary_path The new path to the binary, relative to the test
    ///     suite root.
    /// \param new_root The new test suite root.
    ///
    /// \pre The directory holding the target test program must exist.
    ///     Otherwise, the relocation of the binary will fail.
    void
    move(const char* new_binary_path, const char* new_root)
    {
        _binary_path = fs::path(new_binary_path);
        _root = fs::path(new_root);

        const fs::path src_path = fs::path(_srcdir) / "test_case_plain_helpers";
        const fs::path new_path = _root / _binary_path;
        ATF_REQUIRE(
            ::symlink(src_path.c_str(), new_path.c_str()) != -1);
    }

    /// Runs the helper.
    ///
    /// \param user_config The runtime engine configuration, if different to the
    /// defaults.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(const config::tree& user_config = engine::default_config()) const
    {
        engine::metadata_builder mdbuilder;
        if (_timeout)
            mdbuilder.set_timeout(_timeout.get());
        const engine::test_program test_program(
            "plain", _binary_path, _root, "unit-tests", mdbuilder.build());
        const engine::test_cases_vector& tcs = test_program.test_cases();
        fetch_output_hooks fetcher;
        const engine::test_result result = engine::run_test_case(
            tcs[0].get(), user_config, fetcher, fs::path("."));
        std::cerr << "Result is: " << result << '\n';
        return result;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(test_case__ctor_and_getters)
ATF_TEST_CASE_BODY(test_case__ctor_and_getters)
{
    const engine::metadata md = engine::metadata_builder()
        .add_custom("first", "value")
        .build();
    const engine::test_program test_program(
        "mock", fs::path("abc"), fs::path("unused-root"),
        "unused-suite-name", engine::metadata_builder().build());
    const engine::test_case test_case("mock", test_program, "foo", md);
    ATF_REQUIRE_EQ(&test_program, &test_case.container_test_program());
    ATF_REQUIRE_EQ("foo", test_case.name());
    ATF_REQUIRE(md == test_case.get_metadata());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__fake_result)
ATF_TEST_CASE_BODY(test_case__fake_result)
{
    const engine::test_result result(engine::test_result::skipped,
                                     "Some reason");
    const engine::test_program test_program(
        "mock", fs::path("abc"), fs::path("unused-root"),
        "unused-suite-name", engine::metadata_builder().build());
    const engine::test_case test_case("mock", test_program, "__foo__",
                                      "Some description", result);
    ATF_REQUIRE_EQ(&test_program, &test_case.container_test_program());
    ATF_REQUIRE_EQ("__foo__", test_case.name());
    ATF_REQUIRE(result == test_case.fake_result().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(test_case__operators_eq_and_ne__copy)
{
    const engine::test_program tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());

    const engine::test_case tc1("plain", tp, "name",
                                engine::metadata_builder().build());
    const engine::test_case tc2 = tc1;
    ATF_REQUIRE(  tc1 == tc2);
    ATF_REQUIRE(!(tc1 != tc2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__output);
ATF_TEST_CASE_BODY(test_case__output)
{
    const engine::test_program tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());

    const engine::test_case tc1(
        "plain", tp, "the-name", engine::metadata_builder()
        .add_allowed_platform("foo").add_custom("X-bar", "baz").build());
    std::ostringstream str;
    str << tc1;
    ATF_REQUIRE_EQ(
        "test_case{interface='plain', name='the-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='foo', "
        "custom.X-bar='baz', description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(test_case__operators_eq_and_ne__not_copy)
{
    const std::string base_interface("plain");
    const engine::test_program base_tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        engine::metadata_builder().build());
    const std::string base_name("name");
    const engine::metadata base_metadata = engine::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();

    const engine::test_case base_tc(base_interface, base_tp, base_name,
                                    base_metadata);

    // Construct with all same values.
    {
        const engine::test_case other_tc(base_interface, base_tp, base_name,
                                        base_metadata);

        ATF_REQUIRE(  base_tc == other_tc);
        ATF_REQUIRE(!(base_tc != other_tc));
    }

    // Different interface.
    {
        const engine::test_case other_tc("atf", base_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different test program, different identifier.
    {
        const engine::test_program other_tp(
            "plain", fs::path("another-name"), fs::path("."), "suite2-name",
        engine::metadata_builder().build());
        const engine::test_case other_tc(base_interface, other_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different test program, same identifier.  Cannot be detected!
    {
        const engine::test_program other_tp(
            "plain", fs::path("non-existent"), fs::path("."), "suite2-name",
        engine::metadata_builder().build());
        const engine::test_case other_tc(base_interface, other_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(  base_tc == other_tc);
        ATF_REQUIRE(!(base_tc != other_tc));
    }

    // Different name.
    {
        const engine::test_case other_tc(base_interface, base_tp, "other",
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different metadata.
    {
        const engine::test_case other_tc(base_interface, base_tp, base_name,
                                         engine::metadata_builder().build());

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__current_directory);
ATF_TEST_CASE_BODY(run_test_case__atf__current_directory)
{
    atf_helper helper(this, "pass");
    helper.move("program", ".");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__subdirectory);
ATF_TEST_CASE_BODY(run_test_case__atf__subdirectory)
{
    atf_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    helper.move("dir2/program", "dir1");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__config_variables);
ATF_TEST_CASE_BODY(run_test_case__atf__config_variables)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());

    if (!fs::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__atf__cleanup_shares_workdir)
{
    atf_helper helper(this, "check_cleanup_workdir");
    helper.set_metadata("has_cleanup", "true");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped,
                                       "cookie created"), helper.run());

    if (fs::exists(fs::path("missing_cookie")))
        fail("The cleanup part did not see the cookie; the work directory "
             "is probably not shared");
    if (fs::exists(fs::path("invalid_cookie")))
        fail("The cleanup part read an invalid cookie");
    if (!fs::exists(fs::path("cookie_ok")))
        fail("The cleanup part was not executed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__has_cleanup__atf__false);
ATF_TEST_CASE_BODY(run_test_case__atf__has_cleanup__atf__false)
{
    atf_helper helper(this, "create_cookie_from_cleanup");
    helper.set_metadata("has_cleanup", "false");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The cleanup part was executed even though the test case set "
             "has.cleanup to false");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__has_cleanup__atf__true);
ATF_TEST_CASE_BODY(run_test_case__atf__has_cleanup__atf__true)
{
    atf_helper helper(this, "create_cookie_from_cleanup");
    helper.set_metadata("has_cleanup", "true");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());

    if (!fs::exists(fs::path("cookie")))
        fail("The cleanup part was not executed even though the test case set "
             "has.cleanup to true");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__kill_children);
ATF_TEST_CASE_BODY(run_test_case__atf__kill_children)
{
    atf_helper helper(this, "spawn_blocking_child");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");
    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    int attempts = 30;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        // Looks like the subchild did not die.
        //
        // Note that this might be inaccurate for two reasons:
        // 1) The system may have spawned a new process with the same pid as
        //    our subchild... but in practice, this does not happen because
        //    most systems do not immediately reuse pid numbers.  If that
        //    happens... well, we get a false test failure.
        // 2) We ran so fast that even if the process was sent a signal to
        //    die, it has not had enough time to process it yet.  This is why
        //    we retry this a few times.
        if (attempts > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            --attempts;
            ::usleep(100000);
            goto retry;
        }
        fail(F("The subprocess %s of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__isolation);
ATF_TEST_CASE_BODY(run_test_case__atf__isolation)
{
    atf_helper helper(this, "validate_isolation");
    // Simple checks to make sure that the test case has been isolated.
    utils::setenv("HOME", "fake-value");
    utils::setenv("LANG", "C");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__allowed_architectures);
ATF_TEST_CASE_BODY(run_test_case__atf__allowed_architectures)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("allowed_architectures", "i386 x86_64");
    helper.config().set_string("architecture", "powerpc");
    helper.config().set_string("platform", "");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Current "
                                       "architecture 'powerpc' not supported"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__allowed_platforms);
ATF_TEST_CASE_BODY(run_test_case__atf__allowed_platforms)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("allowed_platforms", "i386 amd64");
    helper.config().set_string("architecture", "");
    helper.config().set_string("platform", "macppc");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Current "
                                       "platform 'macppc' not supported"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__required_configs);
ATF_TEST_CASE_BODY(run_test_case__atf__required_configs)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("required_configs", "used-var");
    helper.set_config("control_dir", fs::current_path());
    helper.set_config("unused-var", "value");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Required "
                                       "configuration property 'used-var' not "
                                       "defined"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__required_programs);
ATF_TEST_CASE_BODY(run_test_case__atf__required_programs)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("required_programs", "/non-existent/program");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Required "
                                       "program '/non-existent/program' not "
                                       "found"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE(run_test_case__atf__required_user__atf__root__atf__ok);
ATF_TEST_CASE_HEAD(run_test_case__atf__required_user__atf__root__atf__ok)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__atf__required_user__atf__root__atf__ok)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "root");
    ATF_REQUIRE(passwd::current_user().is_root());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE(run_test_case__atf__required_user__atf__root__atf__skip);
ATF_TEST_CASE_HEAD(run_test_case__atf__required_user__atf__root__atf__skip)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__atf__required_user__atf__root__atf__skip)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "root");
    ATF_REQUIRE(!passwd::current_user().is_root());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Requires "
                                       "root privileges"),
                   helper.run());
}


ATF_TEST_CASE(run_test_case__atf__required_user__atf__unprivileged__atf__ok);
ATF_TEST_CASE_HEAD(run_test_case__atf__required_user__atf__unprivileged__atf__ok)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__atf__required_user__atf__unprivileged__atf__ok)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "unprivileged");
    ATF_REQUIRE(!helper.config().is_set("unprivileged_user"));
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE(run_test_case__atf__required_user__atf__unprivileged__atf__skip);
ATF_TEST_CASE_HEAD(run_test_case__atf__required_user__atf__unprivileged__atf__skip)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__atf__required_user__atf__unprivileged__atf__skip)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "unprivileged");
    ATF_REQUIRE(!helper.config().is_set("unprivileged_user"));
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::skipped, "Requires "
                                       "an unprivileged user but the "
                                       "unprivileged-user configuration "
                                       "variable is not defined"),
                   helper.run());
}


ATF_TEST_CASE(run_test_case__atf__required_user__atf__unprivileged__atf__drop);
ATF_TEST_CASE_HEAD(run_test_case__atf__required_user__atf__unprivileged__atf__drop)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__atf__required_user__atf__unprivileged__atf__drop)
{
    atf_helper helper(this, "check_unprivileged");
    helper.set_metadata("required_user", "unprivileged");
    helper.config().set< engine::user_node >(
        "unprivileged_user",
        passwd::find_user_by_name(get_config_var("unprivileged-user")));
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__timeout_body);
ATF_TEST_CASE_BODY(run_test_case__atf__timeout_body)
{
    atf_helper helper(this, "timeout_body");
    helper.set_metadata("timeout", "1");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::broken,
                                       "Test case body timed out"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__timeout_cleanup);
ATF_TEST_CASE_BODY(run_test_case__atf__timeout_cleanup)
{
    atf_helper helper(this, "timeout_cleanup");
    helper.set_metadata("has_cleanup", "true");
    helper.set_metadata("timeout", "1");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::broken,
                                       "Test case cleanup timed out"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__stacktrace__atf__body);
ATF_TEST_CASE_BODY(run_test_case__atf__stacktrace__atf__body)
{
    atf_helper helper(this, "crash");
    capture_hooks hooks;
    const engine::test_result result = helper.run(hooks);
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("received signal.*core dumped", result.reason());

    ATF_REQUIRE(!atf::utils::grep_string("attempting to gather stack trace",
                                         hooks.stdout_contents));
    ATF_REQUIRE( atf::utils::grep_string("attempting to gather stack trace",
                                         hooks.stderr_contents));
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__stacktrace__atf__cleanup);
ATF_TEST_CASE_BODY(run_test_case__atf__stacktrace__atf__cleanup)
{
    atf_helper helper(this, "crash_cleanup");
    helper.set_metadata("has_cleanup", "true");
    capture_hooks hooks;
    const engine::test_result result = helper.run(hooks);
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH(F("cleanup received signal %s") % SIGABRT,
                      result.reason());

    ATF_REQUIRE(!atf::utils::grep_string("attempting to gather stack trace",
                                         hooks.stdout_contents));
    ATF_REQUIRE( atf::utils::grep_string("attempting to gather stack trace",
                                         hooks.stderr_contents));
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__atf__missing_results_file)
{
    atf_helper helper(this, "crash");
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    // Need to match instead of doing an explicit comparison because the string
    // may include the "core dumped" substring.
    ATF_REQUIRE_MATCH(F("test case received signal %s") % SIGABRT,
                      result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__atf__missing_test_program)
{
    atf_helper helper(this, "crash");
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    helper.move("test_case_atf_helpers", "dir");
    ATF_REQUIRE(::unlink("dir/test_case_atf_helpers") != -1);
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Test program does not exist", result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__atf__output);
ATF_TEST_CASE_BODY(run_test_case__atf__output)
{
    atf_helper helper(this, "output");
    helper.set_metadata("has_cleanup", "true");

    capture_hooks hooks;
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run(hooks));

    ATF_REQUIRE_EQ("Body message to stdout\nCleanup message to stdout\n",
                   hooks.stdout_contents);
    ATF_REQUIRE_EQ("Body message to stderr\nCleanup message to stderr\n",
                   hooks.stderr_contents);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__result_pass);
ATF_TEST_CASE_BODY(run_test_case__plain__result_pass)
{
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   plain_helper(this, "pass").run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__result_fail);
ATF_TEST_CASE_BODY(run_test_case__plain__result_fail)
{
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::failed,
                                       "Returned non-success exit status 8"),
                   plain_helper(this, "fail").run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__result_crash);
ATF_TEST_CASE_BODY(run_test_case__plain__result_crash)
{
    const engine::test_result result = plain_helper(this, "crash").run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH(F("Received signal %s") % SIGABRT, result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__current_directory);
ATF_TEST_CASE_BODY(run_test_case__plain__current_directory)
{
    plain_helper helper(this, "pass");
    helper.move("program", ".");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__subdirectory);
ATF_TEST_CASE_BODY(run_test_case__plain__subdirectory)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    helper.move("dir2/program", "dir1");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__kill_children);
ATF_TEST_CASE_BODY(run_test_case__plain__kill_children)
{
    plain_helper helper(this, "spawn_blocking_child");
    helper.set("CONTROL_DIR", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");
    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    int attempts = 30;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        // Looks like the subchild did not die.
        //
        // Note that this might be inaccurate for two reasons:
        // 1) The system may have spawned a new process with the same pid as
        //    our subchild... but in practice, this does not happen because
        //    most systems do not immediately reuse pid numbers.  If that
        //    happens... well, we get a false test failure.
        // 2) We ran so fast that even if the process was sent a signal to
        //    die, it has not had enough time to process it yet.  This is why
        //    we retry this a few times.
        if (attempts > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            --attempts;
            ::usleep(100000);
            goto retry;
        }
        fail(F("The subprocess %s of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__isolation);
ATF_TEST_CASE_BODY(run_test_case__plain__isolation)
{
    const plain_helper helper(this, "validate_isolation");
    utils::setenv("TEST_CASE", "validate_isolation");
    // Simple checks to make sure that the test case has been isolated.
    utils::setenv("HOME", "fake-value");
    utils::setenv("LANG", "C");
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::passed),
                   helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__timeout);
ATF_TEST_CASE_BODY(run_test_case__plain__timeout)
{
    plain_helper helper(this, "timeout",
                        utils::make_optional(datetime::delta(1, 0)));
    helper.set("CONTROL_DIR", fs::current_path());
    ATF_REQUIRE_EQ(engine::test_result(engine::test_result::broken,
                                       "Test case timed out"),
                   helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__stacktrace);
ATF_TEST_CASE_BODY(run_test_case__plain__stacktrace)
{
    plain_helper helper(this, "crash");
    helper.set("CONTROL_DIR", fs::current_path());

    const engine::test_result result = plain_helper(this, "crash").run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH(F("Received signal %s") % SIGABRT, result.reason());

    ATF_REQUIRE(!atf::utils::grep_file("attempting to gather stack trace",
                                       "helper-stdout.txt"));
    ATF_REQUIRE( atf::utils::grep_file("attempting to gather stack trace",
                                       "helper-stderr.txt"));
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__plain__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__plain__missing_test_program)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    helper.move("test_case_helpers", "dir");
    ATF_REQUIRE(::unlink("dir/test_case_helpers") != -1);
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Test program does not exist", result.reason());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_case__ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, test_case__fake_result);

    ATF_ADD_TEST_CASE(tcs, test_case__operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, test_case__operators_eq_and_ne__not_copy);

    ATF_ADD_TEST_CASE(tcs, test_case__output);

    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__current_directory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__config_variables);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__cleanup_shares_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__has_cleanup__atf__false);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__has_cleanup__atf__true);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__kill_children);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__isolation);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__allowed_architectures);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__allowed_platforms);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_configs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_programs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_user__atf__root__atf__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_user__atf__root__atf__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_user__atf__unprivileged__atf__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_user__atf__unprivileged__atf__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__required_user__atf__unprivileged__atf__drop);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__timeout_body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__timeout_cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__stacktrace__atf__body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__stacktrace__atf__cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__missing_test_program);
    ATF_ADD_TEST_CASE(tcs, run_test_case__atf__output);

    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__result_pass);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__result_fail);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__result_crash);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__current_directory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__kill_children);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__isolation);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__timeout);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__stacktrace);
    ATF_ADD_TEST_CASE(tcs, run_test_case__plain__missing_test_program);

    // TODO(jmmv): Add test cases for debug.
}
