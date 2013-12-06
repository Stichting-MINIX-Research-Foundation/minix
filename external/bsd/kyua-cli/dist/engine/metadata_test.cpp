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

#include "engine/metadata.hpp"

#include <sstream>

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/memory.hpp"
#include "utils/passwd.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(defaults);
ATF_TEST_CASE_BODY(defaults)
{
    const engine::metadata md = engine::metadata_builder().build();
    ATF_REQUIRE(md.allowed_architectures().empty());
    ATF_REQUIRE(md.allowed_platforms().empty());
    ATF_REQUIRE(md.allowed_platforms().empty());
    ATF_REQUIRE(md.custom().empty());
    ATF_REQUIRE(md.description().empty());
    ATF_REQUIRE(!md.has_cleanup());
    ATF_REQUIRE(md.required_configs().empty());
    ATF_REQUIRE(md.required_files().empty());
    ATF_REQUIRE_EQ(units::bytes(0), md.required_memory());
    ATF_REQUIRE(md.required_programs().empty());
    ATF_REQUIRE(md.required_user().empty());
    ATF_REQUIRE(engine::default_timeout == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(add);
ATF_TEST_CASE_BODY(add)
{
    engine::strings_set architectures;
    architectures.insert("1-architecture");
    architectures.insert("2-architecture");

    engine::strings_set platforms;
    platforms.insert("1-platform");
    platforms.insert("2-platform");

    engine::properties_map custom;
    custom["1-custom"] = "first";
    custom["2-custom"] = "second";

    engine::strings_set configs;
    configs.insert("1-config");
    configs.insert("2-config");

    engine::paths_set files;
    files.insert(fs::path("1-file"));
    files.insert(fs::path("2-file"));

    engine::paths_set programs;
    programs.insert(fs::path("1-program"));
    programs.insert(fs::path("2-program"));

    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .add_custom("1-custom", "first")
        .add_custom("2-custom", "second")
        .add_required_config("1-config")
        .add_required_file(fs::path("1-file"))
        .add_required_program(fs::path("1-program"))
        .add_allowed_architecture("2-architecture")
        .add_allowed_platform("2-platform")
        .add_required_config("2-config")
        .add_required_file(fs::path("2-file"))
        .add_required_program(fs::path("2-program"))
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE(programs == md.required_programs());
}


ATF_TEST_CASE_WITHOUT_HEAD(copy);
ATF_TEST_CASE_BODY(copy)
{
    const engine::metadata md1 = engine::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .build();

    const engine::metadata md2 = engine::metadata_builder(md1)
        .add_allowed_architecture("2-architecture")
        .build();

    ATF_REQUIRE_EQ(1, md1.allowed_architectures().size());
    ATF_REQUIRE_EQ(2, md2.allowed_architectures().size());
    ATF_REQUIRE_EQ(1, md1.allowed_platforms().size());
    ATF_REQUIRE_EQ(1, md2.allowed_platforms().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_setters);
ATF_TEST_CASE_BODY(override_all_with_setters)
{
    engine::strings_set architectures;
    architectures.insert("the-architecture");

    engine::strings_set platforms;
    platforms.insert("the-platforms");

    engine::properties_map custom;
    custom["first"] = "hello";
    custom["second"] = "bye";

    const std::string description = "Some long text";

    engine::strings_set configs;
    configs.insert("the-configs");

    engine::paths_set files;
    files.insert(fs::path("the-files"));

    const units::bytes memory(12345);

    engine::paths_set programs;
    programs.insert(fs::path("the-programs"));

    const std::string user = "root";

    const datetime::delta timeout(123, 0);

    const engine::metadata md = engine::metadata_builder()
        .set_allowed_architectures(architectures)
        .set_allowed_platforms(platforms)
        .set_custom(custom)
        .set_description(description)
        .set_has_cleanup(true)
        .set_required_configs(configs)
        .set_required_files(files)
        .set_required_memory(memory)
        .set_required_programs(programs)
        .set_required_user(user)
        .set_timeout(timeout)
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE_EQ(description, md.description());
    ATF_REQUIRE(md.has_cleanup());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
    ATF_REQUIRE(timeout == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_set_string);
ATF_TEST_CASE_BODY(override_all_with_set_string)
{
    engine::strings_set architectures;
    architectures.insert("a1");
    architectures.insert("a2");

    engine::strings_set platforms;
    platforms.insert("p1");
    platforms.insert("p2");

    engine::properties_map custom;
    custom["user-defined"] = "the-value";

    const std::string description = "Another long text";

    engine::strings_set configs;
    configs.insert("config-var");

    engine::paths_set files;
    files.insert(fs::path("plain"));
    files.insert(fs::path("/absolute/path"));

    const units::bytes memory(1024 * 1024);

    engine::paths_set programs;
    programs.insert(fs::path("program"));
    programs.insert(fs::path("/absolute/prog"));

    const std::string user = "unprivileged";

    const datetime::delta timeout(45, 0);

    const engine::metadata md = engine::metadata_builder()
        .set_string("allowed_architectures", "a1 a2")
        .set_string("allowed_platforms", "p1 p2")
        .set_string("custom.user-defined", "the-value")
        .set_string("description", "Another long text")
        .set_string("has_cleanup", "true")
        .set_string("required_configs", "config-var")
        .set_string("required_files", "plain /absolute/path")
        .set_string("required_memory", "1M")
        .set_string("required_programs", "program /absolute/prog")
        .set_string("required_user", "unprivileged")
        .set_string("timeout", "45")
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE_EQ(description, md.description());
    ATF_REQUIRE(md.has_cleanup());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
    ATF_REQUIRE(timeout == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__empty);
ATF_TEST_CASE_BODY(operators_eq_and_ne__empty)
{
    const engine::metadata md1 = engine::metadata_builder().build();
    const engine::metadata md2 = engine::metadata_builder().build();
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__copy)
{
    const engine::metadata md1 = engine::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();
    const engine::metadata md2 = md1;
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__equal);
ATF_TEST_CASE_BODY(operators_eq_and_ne__equal)
{
    const engine::metadata md1 = engine::metadata_builder()
        .add_allowed_architecture("a")
        .add_allowed_architecture("b")
        .add_custom("X-foo", "bar")
        .build();
    const engine::metadata md2 = engine::metadata_builder()
        .add_allowed_architecture("b")
        .add_allowed_architecture("a")
        .add_custom("X-foo", "bar")
        .build();
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__different);
ATF_TEST_CASE_BODY(operators_eq_and_ne__different)
{
    const engine::metadata md1 = engine::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();
    const engine::metadata md2 = engine::metadata_builder()
        .add_custom("X-foo", "bar")
        .add_custom("X-baz", "foo bar")
        .build();
    ATF_REQUIRE(!(md1 == md2));
    ATF_REQUIRE(  md1 != md2);
}


ATF_TEST_CASE_WITHOUT_HEAD(output__defaults);
ATF_TEST_CASE_BODY(output__defaults)
{
    std::ostringstream str;
    str << engine::metadata_builder().build();
    ATF_REQUIRE_EQ("metadata{allowed_architectures='', allowed_platforms='', "
                   "description='', has_cleanup='false', required_configs='', "
                   "required_files='', required_memory='0', "
                   "required_programs='', required_user='', timeout='300'}",
                   str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_values);
ATF_TEST_CASE_BODY(output__some_values)
{
    std::ostringstream str;
    str << engine::metadata_builder()
        .add_allowed_architecture("abc")
        .add_required_file(fs::path("foo"))
        .add_required_file(fs::path("bar"))
        .set_required_memory(units::bytes(1024))
        .build();
    ATF_REQUIRE_EQ(
        "metadata{allowed_architectures='abc', allowed_platforms='', "
        "description='', has_cleanup='false', required_configs='', "
        "required_files='bar foo', required_memory='1.00K', "
        "required_programs='', required_user='', timeout='300'}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__none);
ATF_TEST_CASE_BODY(check_reqs__none)
{
    const engine::metadata md = engine::metadata_builder().build();
    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__one_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__one_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("x86_64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "x86_64");
    user_config.set_string("platform", "");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__one_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__one_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("x86_64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "i386");
    user_config.set_string("platform", "");
    ATF_REQUIRE_MATCH("Current architecture 'i386' not supported",
                      engine::check_reqs(md, user_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__many_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__many_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("x86_64")
        .add_allowed_architecture("i386")
        .add_allowed_architecture("powerpc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "i386");
    user_config.set_string("platform", "");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__many_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__many_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("x86_64")
        .add_allowed_architecture("i386")
        .add_allowed_architecture("powerpc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "arm");
    user_config.set_string("platform", "");
    ATF_REQUIRE_MATCH("Current architecture 'arm' not supported",
                      engine::check_reqs(md, user_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__one_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__one_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_platform("amd64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "amd64");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__one_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__one_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_platform("amd64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "i386");
    ATF_REQUIRE_MATCH("Current platform 'i386' not supported",
                      engine::check_reqs(md, user_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__many_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__many_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_platform("amd64")
        .add_allowed_platform("i386")
        .add_allowed_platform("macppc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "i386");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__many_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__many_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_allowed_platform("amd64")
        .add_allowed_platform("i386")
        .add_allowed_platform("macppc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "shark");
    ATF_REQUIRE_MATCH("Current platform 'shark' not supported",
                      engine::check_reqs(md, user_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__one_ok);
ATF_TEST_CASE_BODY(check_reqs__required_configs__one_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_config("my-var")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.my-var", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "suite").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__one_fail);
ATF_TEST_CASE_BODY(check_reqs__required_configs__one_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_config("unprivileged_user")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.my-var", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE_MATCH("Required configuration property 'unprivileged_user' not "
                      "defined",
                      engine::check_reqs(md, user_config, "suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__many_ok);
ATF_TEST_CASE_BODY(check_reqs__required_configs__many_ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_config("foo")
        .add_required_config("bar")
        .add_required_config("baz")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.foo", "value2");
    user_config.set_string("test_suites.suite.bar", "value3");
    user_config.set_string("test_suites.suite.baz", "value4");
    user_config.set_string("test_suites.suite.zzz", "value5");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "suite").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__many_fail);
ATF_TEST_CASE_BODY(check_reqs__required_configs__many_fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_config("foo")
        .add_required_config("bar")
        .add_required_config("baz")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.foo", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE_MATCH("Required configuration property 'bar' not defined",
                      engine::check_reqs(md, user_config, "suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__special);
ATF_TEST_CASE_BODY(check_reqs__required_configs__special)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_config("unprivileged-user")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE_MATCH("Required configuration property 'unprivileged-user' "
                      "not defined",
                      engine::check_reqs(md, user_config, ""));
    user_config.set< engine::user_node >(
        "unprivileged_user", passwd::user("foo", 1, 2));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "foo").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__root__ok);
ATF_TEST_CASE_BODY(check_reqs__required_user__root__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_user("root")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__root__fail);
ATF_TEST_CASE_BODY(check_reqs__required_user__root__fail)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_user("root")
        .build();

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE_MATCH("Requires root privileges",
                      engine::check_reqs(md, engine::empty_config(), ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__same);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__same)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__ok);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set< engine::user_node >(
        "unprivileged_user", passwd::user("", 123, 1));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__fail);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__fail)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE_MATCH("Requires.*unprivileged.*unprivileged-user",
                      engine::check_reqs(md, user_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_files__ok);
ATF_TEST_CASE_BODY(check_reqs__required_files__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_file(fs::current_path() / "test-file")
        .build();

    atf::utils::create_file("test-file", "");

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_files__fail);
ATF_TEST_CASE_BODY(check_reqs__required_files__fail)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_file(fs::path("/non-existent/file"))
        .build();

    ATF_REQUIRE_MATCH("'/non-existent/file' not found$",
                      engine::check_reqs(md, engine::empty_config(), ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_memory__ok);
ATF_TEST_CASE_BODY(check_reqs__required_memory__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_memory(units::bytes::parse("1m"))
        .build();

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_memory__fail);
ATF_TEST_CASE_BODY(check_reqs__required_memory__fail)
{
    const engine::metadata md = engine::metadata_builder()
        .set_required_memory(units::bytes::parse("100t"))
        .build();

    if (utils::physical_memory() == 0)
        skip("Don't know how to query the amount of physical memory");
    ATF_REQUIRE_MATCH("Requires 100.00T .*memory",
                      engine::check_reqs(md, engine::empty_config(), ""));
}


ATF_TEST_CASE(check_reqs__required_programs__ok);
ATF_TEST_CASE_HEAD(check_reqs__required_programs__ok)
{
    set_md_var("require.progs", "/bin/ls /bin/mv");
}
ATF_TEST_CASE_BODY(check_reqs__required_programs__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("foo"))
        .add_required_program(fs::path("/bin/mv"))
        .build();

    fs::mkdir(fs::path("bin"), 0755);
    atf::utils::create_file("bin/foo", "");
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_programs__fail_absolute);
ATF_TEST_CASE_BODY(check_reqs__required_programs__fail_absolute)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_program(fs::path("/non-existent/program"))
        .build();

    ATF_REQUIRE_MATCH("'/non-existent/program' not found$",
                      engine::check_reqs(md, engine::empty_config(), ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_programs__fail_relative);
ATF_TEST_CASE_BODY(check_reqs__required_programs__fail_relative)
{
    const engine::metadata md = engine::metadata_builder()
        .add_required_program(fs::path("foo"))
        .add_required_program(fs::path("bar"))
        .build();

    fs::mkdir(fs::path("bin"), 0755);
    atf::utils::create_file("bin/foo", "");
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    ATF_REQUIRE_MATCH("'bar' not found in PATH$",
                      engine::check_reqs(md, engine::empty_config(), ""));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, defaults);
    ATF_ADD_TEST_CASE(tcs, add);
    ATF_ADD_TEST_CASE(tcs, copy);
    ATF_ADD_TEST_CASE(tcs, override_all_with_setters);
    ATF_ADD_TEST_CASE(tcs, override_all_with_set_string);

    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__empty);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__equal);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__different);

    ATF_ADD_TEST_CASE(tcs, output__defaults);
    ATF_ADD_TEST_CASE(tcs, output__some_values);

    // TODO(jmmv): Add tests for error conditions (invalid keys and invalid
    // values).

    ATF_ADD_TEST_CASE(tcs, check_reqs__none);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__special);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__root__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__same);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_files__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_files__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_memory__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_memory__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__fail_absolute);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__fail_relative);
}
