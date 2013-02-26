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

#include "engine/kyuafile.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "engine/exceptions.hpp"
#include "engine/test_program.hpp"
#include "engine/testers.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/lua_module.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;

using utils::none;
using utils::optional;


// History of Kyuafile file versions:
//
// 2 - Changed the syntax() call to take only a version number, instead of the
//     word 'config' as the first argument and the version as the second one.
//     Files now start with syntax(2) instead of syntax('kyuafile', 1).
//
// 1 - Initial version.


namespace {


static int lua_atf_test_program(lutok::state&);
static int lua_current_kyuafile(lutok::state&);
static int lua_include(lutok::state&);
static int lua_plain_test_program(lutok::state&);
static int lua_syntax(lutok::state&);
static int lua_test_suite(lutok::state&);


/// Concatenates two paths while avoiding paths to start with './'.
///
/// \param root Path to the directory containing the file.
/// \param file Path to concatenate to root.  Cannot be absolute.
///
/// \return The concatenated path.
static fs::path
relativize(const fs::path& root, const fs::path& file)
{
    PRE(!file.is_absolute());

    if (root == fs::path("."))
        return file;
    else
        return root / file;
}


/// Implementation of a parser for Kyuafiles.
///
/// The main purpose of having this as a class is to keep track of global state
/// within the Lua files and allowing the Lua callbacks to easily access such
/// data.
class parser : utils::noncopyable {
    /// Lua state to parse a single Kyuafile file.
    lutok::state _state;

    /// Root directory of the test suite represented by the Kyuafile.
    const fs::path _source_root;

    /// Root directory of the test programs.
    const fs::path _build_root;

    /// Name of the Kyuafile to load relative to _source_root.
    const fs::path _relative_filename;

    /// Version of the Kyuafile file format requested by the parsed file.
    ///
    /// This is set once the Kyuafile invokes the syntax() call.
    optional< int > _version;

    /// Name of the test suite defined by the Kyuafile.
    ///
    /// This is set once the Kyuafile invokes the test_suite() call.
    optional< std::string > _test_suite;

    /// Collection of test programs defined by the Kyuafile.
    ///
    /// This acts as an accumulator for all the *_test_program() calls within
    /// the Kyuafile.
    engine::test_programs_vector _test_programs;

public:
    /// Initializes the parser and the Lua state.
    ///
    /// \param source_root_ The root directory of the test suite represented by
    ///     the Kyuafile.
    /// \param build_root_ The root directory of the test programs.
    /// \param relative_filename_ Name of the Kyuafile to load relative to
    ///     source_root_.
    parser(const fs::path& source_root_, const fs::path& build_root_,
           const fs::path& relative_filename_) :
        _source_root(source_root_), _build_root(build_root_),
        _relative_filename(relative_filename_)
    {
        lutok::stack_cleaner cleaner(_state);

        _state.push_cxx_function(lua_syntax);
        _state.set_global("syntax");
        *_state.new_userdata< parser* >() = this;
        _state.set_global("_parser");

        _state.push_cxx_function(lua_atf_test_program);
        _state.set_global("atf_test_program");
        _state.push_cxx_function(lua_current_kyuafile);
        _state.set_global("current_kyuafile");
        _state.push_cxx_function(lua_include);
        _state.set_global("include");
        _state.push_cxx_function(lua_plain_test_program);
        _state.set_global("plain_test_program");
        _state.push_cxx_function(lua_test_suite);
        _state.set_global("test_suite");

        _state.open_base();
        _state.open_string();
        _state.open_table();
        fs::open_fs(_state);
    }

    /// Destructor.
    ~parser(void)
    {
    }

    /// Gets the parser object associated to a Lua state.
    ///
    /// \param state The Lua state from which to obtain the parser object.
    ///
    /// \return A pointer to the parser.
    static parser*
    get_from_state(lutok::state& state)
    {
        lutok::stack_cleaner cleaner(state);
        state.get_global("_parser");
        return *state.to_userdata< parser* >();
    }

    /// Callback for the Kyuafile current_kyuafile() function.
    ///
    /// \return Returns the absolute path to the current Kyuafile.
    fs::path
    callback_current_kyuafile(void) const
    {
        const fs::path file = relativize(_source_root, _relative_filename);
        if (file.is_absolute())
            return file;
        else
            return file.to_absolute();
    }

    /// Callback for the Kyuafile include() function.
    ///
    /// \post _test_programs is extended with the the test programs defined by
    /// the included file.
    ///
    /// \param raw_file Path to the file to include.
    void
    callback_include(const fs::path& raw_file)
    {
        const fs::path file = relativize(_relative_filename.branch_path(),
                                         raw_file);
        const engine::test_programs_vector subtps =
            parser(_source_root, _build_root, file).parse();

        std::copy(subtps.begin(), subtps.end(),
                  std::back_inserter(_test_programs));
    }

    /// Callback for the Kyuafile syntax() function.
    ///
    /// \post _version is set to the requested version.
    ///
    /// \param version Version of the Kyuafile syntax requested by the file.
    ///
    /// \throw std::runtime_error If the format or the version are invalid, or
    /// if syntax() has already been called.
    void
    callback_syntax(const int version)
    {
        if (_version)
            throw std::runtime_error("Can only call syntax() once");

        if (version < 1 || version > 2)
            throw std::runtime_error(F("Unsupported file version %s") %
                                     version);

        _version = utils::make_optional(version);
    }

    /// Callback for the various Kyuafile *_test_program() functions.
    ///
    /// \post _test_programs is extended to include the newly defined test
    /// program.
    ///
    /// \param interface Name of the test program interface.
    /// \param raw_path Path to the test program, relative to the Kyuafile.
    ///     This has to be adjusted according to the relative location of this
    ///     Kyuafile to _source_root.
    /// \param test_suite_override Name of the test suite this test program
    ///     belongs to, if explicitly defined at the test program level.
    /// \param metadata Metadata variables passed to the test program.
    ///
    /// \throw std::runtime_error If the test program definition is invalid or
    ///     if the test program does not exist.
    void
    callback_test_program(const std::string& interface,
                          const fs::path& raw_path,
                          const std::string& test_suite_override,
                          const engine::metadata& metadata)
    {
        if (raw_path.is_absolute())
            throw std::runtime_error(F("Got unexpected absolute path for test "
                                       "program '%s'") % raw_path);
        else if (raw_path.str() != raw_path.leaf_name())
            throw std::runtime_error(F("Test program '%s' cannot contain path "
                                       "components") % raw_path);

        const fs::path path = relativize(_relative_filename.branch_path(),
                                         raw_path);

        if (!fs::exists(_build_root / path))
            throw std::runtime_error(F("Non-existent test program '%s'") %
                                     path);

        const std::string test_suite = test_suite_override.empty()
            ? _test_suite.get() : test_suite_override;
        _test_programs.push_back(engine::test_program_ptr(
            new engine::test_program(interface, path, _build_root, test_suite,
                                     metadata)));
    }

    /// Callback for the Kyuafile test_suite() function.
    ///
    /// \post _version is set to the requested version.
    ///
    /// \param name Name of the test suite.
    ///
    /// \throw std::runtime_error If test_suite() has already been called.
    void
    callback_test_suite(const std::string& name)
    {
        if (_test_suite)
            throw std::runtime_error("Can only call test_suite() once");
        _test_suite = utils::make_optional(name);
    }

    /// Parses the Kyuafile.
    ///
    /// \pre Can only be invoked once.
    ///
    /// \return The collection of test programs defined by the Kyuafile.
    ///
    /// \throw load_error If there is any problem parsing the file.
    const engine::test_programs_vector&
    parse(void)
    {
        PRE(_test_programs.empty());

        const fs::path load_path = relativize(_source_root, _relative_filename);
        try {
            lutok::do_file(_state, load_path.str());
        } catch (const std::runtime_error& e) {
            // It is tempting to think that all of our various auxiliary
            // functions above could raise load_error by themselves thus making
            // this exception rewriting here unnecessary.  Howver, that would
            // not work because the helper functions above are executed within a
            // Lua context, and we lose their type when they are propagated out
            // of it.
            throw engine::load_error(load_path, e.what());
        }

        if (!_version)
            throw engine::load_error(load_path, "syntax() never called");

        return _test_programs;
    }
};


/// Gets a string field from a Lua table.
///
/// \pre state(-1) contains a table.
///
/// \param state The Lua state.
/// \param field The name of the field to query.
/// \param error The error message to raise when an error condition is
///     encoutered.
///
/// \return The string value from the table.
///
/// \throw std::runtime_error If there is any problem accessing the table.
static inline std::string
get_table_string(lutok::state& state, const char* field,
                 const std::string& error)
{
    PRE(state.is_table());

    lutok::stack_cleaner cleaner(state);

    state.push_string(field);
    state.get_table();
    if (!state.is_string())
        throw std::runtime_error(error);
    return state.to_string();
}


/// Checks if the given interface name is valid.
///
/// \param interface The name of the interface to validate.
///
/// \throw std::runtime_error If the given interface is not supported.
static void
ensure_valid_interface(const std::string& interface)
{
    try {
        (void)engine::tester_path(interface);
    } catch (const engine::error& e) {
        throw std::runtime_error(F("Unsupported test interface '%s'") %
                                 interface);
    }
}


/// Glue to invoke parser::callback_test_program() from Lua.
///
/// This is a helper function for the various *_test_program() calls, as they
/// only differ in the interface of the defined test program.
///
/// \pre state(-1) A table with the arguments that define the test program.  The
/// special argument 'test_suite' provides an override to the global test suite
/// name.  The rest of the arguments are part of the test program metadata.
///
/// \param state The Lua state that executed the function.
/// \param interface Name of the test program interface.
///
/// \return Number of return values left on the Lua stack.
///
/// \throw std::runtime_error If the arguments to the function are invalid.
static int
lua_generic_test_program(lutok::state& state, const std::string& interface)
{
    if (!state.is_table())
        throw std::runtime_error(
            F("%s_test_program expects a table of properties as its single "
              "argument") % interface);

    ensure_valid_interface(interface);

    lutok::stack_cleaner cleaner(state);

    state.push_string("name");
    state.get_table();
    if (!state.is_string())
        throw std::runtime_error("Test program name not defined or not a "
                                 "string");
    const fs::path path(state.to_string());
    state.pop(1);

    state.push_string("test_suite");
    state.get_table();
    std::string test_suite;
    if (state.is_nil()) {
        // Leave empty to use the global test-suite value.
    } else if (state.is_string()) {
        test_suite = state.to_string();
    } else {
        throw std::runtime_error(F("Found non-string value in the test_suite "
                                   "property of test program '%s'") % path);
    }
    state.pop(1);

    engine::metadata_builder mdbuilder;
    state.push_nil();
    while (state.next()) {
        if (!state.is_string(-2))
            throw std::runtime_error(F("Found non-string metadata property "
                                       "name in test program '%s'") %
                                     path);
        const std::string property = state.to_string(-2);

        if (property != "name" && property != "test_suite") {
            if (!state.is_number(-1) && !state.is_string(-1))
                throw std::runtime_error(
                    F("Metadata property '%s' in test program '%s' cannot be "
                      "converted to a string") % property % path);
            const std::string value = state.to_string(-1);

            mdbuilder.set_string(property, value);
        }

        state.pop(1);
    }

    parser::get_from_state(state)->callback_test_program(
        interface, path, test_suite, mdbuilder.build());
    return 0;
}


/// Specialization of lua_generic_test_program for ATF test programs.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_atf_test_program(lutok::state& state)
{
    return lua_generic_test_program(state, "atf");
}


/// Glue to invoke parser::callback_current_kyuafile() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_current_kyuafile(lutok::state& state)
{
    state.push_string(parser::get_from_state(state)->
                      callback_current_kyuafile().str());
    return 1;
}


/// Glue to invoke parser::callback_include() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_include(lutok::state& state)
{
    parser::get_from_state(state)->callback_include(
        fs::path(state.to_string()));
    return 0;
}


/// Specialization of lua_generic_test_program for plain test programs.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_plain_test_program(lutok::state& state)
{
    return lua_generic_test_program(state, "plain");
}


/// Glue to invoke parser::callback_syntax() from Lua.
///
/// \pre state(-2) The syntax format name, if a v1 file.
/// \pre state(-1) The syntax format version.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_syntax(lutok::state& state)
{
    if (!state.is_number(-1))
        throw std::runtime_error("Last argument to syntax must be a number");
    const int syntax_version = state.to_integer(-1);

    if (syntax_version == 1) {
        if (state.get_top() != 2)
            throw std::runtime_error("Version 1 files need two arguments to "
                                     "syntax()");
        if (!state.is_string(-2) || state.to_string(-2) != "kyuafile")
            throw std::runtime_error("First argument to syntax must be "
                                     "'kyuafile' for version 1 files");
    } else {
        if (state.get_top() != 1)
            throw std::runtime_error("syntax() only takes one argument");
    }

    parser::get_from_state(state)->callback_syntax(syntax_version);
    return 0;
}


/// Glue to invoke parser::callback_test_suite() from Lua.
///
/// \param state The Lua state that executed the function.
///
/// \return Number of return values left on the Lua stack.
static int
lua_test_suite(lutok::state& state)
{
    parser::get_from_state(state)->callback_test_suite(state.to_string());
    return 0;
}


}  // anonymous namespace


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param source_root_ The root directory for the test suite represented by the
///     Kyuafile.  In other words, the directory containing the first Kyuafile
///     processed.
/// \param build_root_ The root directory for the test programs themselves.  In
///     general, this will be the same as source_root_.  If different, the
///     specified directory must follow the exact same layout of source_root_.
/// \param tps_ Collection of test programs that belong to this test suite.
engine::kyuafile::kyuafile(const fs::path& source_root_,
                           const fs::path& build_root_,
                           const test_programs_vector& tps_) :
    _source_root(source_root_),
    _build_root(build_root_),
    _test_programs(tps_)
{
}


/// Destructor.
engine::kyuafile::~kyuafile(void)
{
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
/// \param user_build_root If not none, specifies a path to a directory
///     containing the test programs themselves.  The layout of the build root
///     must match the layout of the source root (which is just the directory
///     from which the Kyuafile is being read).
///
/// \return High-level representation of the configuration file.
///
/// \throw load_error If there is any problem loading the file.  This includes
///     file access errors and syntax errors.
engine::kyuafile
engine::kyuafile::load(const fs::path& file,
                       const optional< fs::path > user_build_root)
{
    const fs::path source_root_ = file.branch_path();
    const fs::path build_root_ = user_build_root ?
        user_build_root.get() : source_root_;

    return kyuafile(source_root_, build_root_,
                    parser(source_root_, build_root_,
                           fs::path(file.leaf_name())).parse());
}


/// Gets the root directory of the test suite.
///
/// \return A path.
const fs::path&
engine::kyuafile::source_root(void) const
{
    return _source_root;
}


/// Gets the root directory of the test programs.
///
/// \return A path.
const fs::path&
engine::kyuafile::build_root(void) const
{
    return _build_root;
}


/// Gets the collection of test programs that belong to this test suite.
///
/// \return Collection of test program executable names.
const engine::test_programs_vector&
engine::kyuafile::test_programs(void) const
{
    return _test_programs;
}
