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

#include <memory>

#include "engine/exceptions.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/nodes.ipp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/memory.hpp"
#include "utils/passwd.hpp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace text = utils::text;
namespace units = utils::units;


/// The default timeout value for test cases that do not provide one.
/// TODO(jmmv): We should not be doing this; see issue 5 for details.
datetime::delta engine::default_timeout(300, 0);


namespace {


/// A leaf node that holds a bytes quantity.
class bytes_node : public config::native_leaf_node< units::bytes > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::auto_ptr< bytes_node > new_node(new bytes_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param unused_state The Lua state onto which to push the value.
    void
    push_lua(lutok::state& UTILS_UNUSED_PARAM(state)) const
    {
        UNREACHABLE;
    }

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param unused_state The Lua state from which to get the value.
    /// \param unused_index The stack index in which the value resides.
    void
    set_lua(lutok::state& UTILS_UNUSED_PARAM(state),
            const int UTILS_UNUSED_PARAM(index))
    {
        UNREACHABLE;
    }
};


/// A leaf node that holds a time delta.
class delta_node : public config::typed_leaf_node< datetime::delta > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::auto_ptr< delta_node > new_node(new delta_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Sets the value of the node from a raw string representation.
    ///
    /// \param raw_value The value to set the node to.
    ///
    /// \throw value_error If the value is invalid.
    void
    set_string(const std::string& raw_value)
    {
        unsigned int seconds;
        try {
            seconds = text::to_type< unsigned int >(raw_value);
        } catch (const text::error& e) {
            throw config::value_error(F("Invalid time delta %s") % raw_value);
        }
        set(datetime::delta(seconds, 0));
    }

    /// Converts the contents of the node to a string.
    ///
    /// \pre The node must have a value.
    ///
    /// \return A string representation of the value held by the node.
    std::string
    to_string(void) const
    {
        return F("%s") % value().seconds;
    }

    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param unused_state The Lua state onto which to push the value.
    void
    push_lua(lutok::state& UTILS_UNUSED_PARAM(state)) const
    {
        UNREACHABLE;
    }

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param unused_state The Lua state from which to get the value.
    /// \param unused_index The stack index in which the value resides.
    void
    set_lua(lutok::state& UTILS_UNUSED_PARAM(state),
            const int UTILS_UNUSED_PARAM(index))
    {
        UNREACHABLE;
    }
};


/// A leaf node that holds a "required user" property.
///
/// This node is just a string, but it provides validation of the only allowed
/// values.
class user_node : public config::string_node {
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::auto_ptr< user_node > new_node(new user_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Checks a given user textual representation for validity.
    ///
    /// \param user The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& user) const
    {
        if (!user.empty() && user != "root" && user != "unprivileged")
            throw config::value_error("Invalid required user value");
    }
};


/// A leaf node that holds a set of paths.
///
/// This node type is used to represent the value of the required files and
/// required programs, for example, and these do not allow relative paths.  We
/// check this here.
class paths_set_node : public config::base_set_node< fs::path > {
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::auto_ptr< paths_set_node > new_node(new paths_set_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Converts a single path to the native type.
    ///
    /// \param raw_value The value to parse.
    ///
    /// \return The parsed value.
    ///
    /// \throw config::value_error If the value is invalid.
    fs::path
    parse_one(const std::string& raw_value) const
    {
        try {
            return fs::path(raw_value);
        } catch (const fs::error& e) {
            throw config::value_error(e.what());
        }
    }

    /// Checks a collection of paths for validity.
    ///
    /// \param paths The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& paths) const
    {
        for (value_type::const_iterator iter = paths.begin();
             iter != paths.end(); ++iter) {
            const fs::path& path = *iter;
            if (!path.is_absolute() && path.ncomponents() > 1)
                throw config::value_error(F("Relative path '%s' not allowed") %
                                          *iter);
        }
    }
};


/// Initializes a tree to hold test case requirements.
///
/// \param [in,out] tree The tree to initialize.
static void
init_tree(config::tree& tree)
{
    tree.define< config::strings_set_node >("allowed_architectures");
    tree.set< config::strings_set_node >("allowed_architectures",
                                         engine::strings_set());

    tree.define< config::strings_set_node >("allowed_platforms");
    tree.set< config::strings_set_node >("allowed_platforms",
                                         engine::strings_set());

    tree.define_dynamic("custom");

    tree.define< config::string_node >("description");
    tree.set< config::string_node >("description", "");

    tree.define< config::bool_node >("has_cleanup");
    tree.set< config::bool_node >("has_cleanup", false);

    tree.define< config::strings_set_node >("required_configs");
    tree.set< config::strings_set_node >("required_configs",
                                         engine::strings_set());

    tree.define< paths_set_node >("required_files");
    tree.set< paths_set_node >("required_files", engine::paths_set());

    tree.define< bytes_node >("required_memory");
    tree.set< bytes_node >("required_memory", units::bytes(0));

    tree.define< paths_set_node >("required_programs");
    tree.set< paths_set_node >("required_programs", engine::paths_set());

    tree.define< user_node >("required_user");
    tree.set< user_node >("required_user", "");

    tree.define< delta_node >("timeout");
    tree.set< delta_node >("timeout", engine::default_timeout);
}


/// Looks up a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
///
/// \return A read-write reference to the value in the node.
///
/// \throw engine::error If the key is not known or if the value is not valid.
template< class NodeType >
typename NodeType::value_type&
lookup_rw(config::tree& tree, const std::string& key)
{
    try {
        return tree.lookup_rw< NodeType >(key);
    } catch (const config::unknown_key_error& e) {
        throw engine::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


/// Sets a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
/// \param value The value to set the node to.
///
/// \throw engine::error If the key is not known or if the value is not valid.
template< class NodeType >
void
set(config::tree& tree, const std::string& key,
    const typename NodeType::value_type& value)
{
    try {
        tree.set< NodeType >(key, value);
    } catch (const config::unknown_key_error& e) {
        throw engine::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


/// Checks if all required configuration variables are present.
///
/// \param required_configs Set of required variable names.
/// \param user_config Runtime user configuration.
/// \param test_suite_name Name of the test suite the test belongs to.
///
/// \return Empty if all variables are present or an error message otherwise.
static std::string
check_required_configs(const engine::strings_set& required_configs,
                       const config::tree& user_config,
                       const std::string& test_suite_name)
{
    for (engine::strings_set::const_iterator iter = required_configs.begin();
         iter != required_configs.end(); iter++) {
        std::string property;
        // TODO(jmmv): All this rewrite logic belongs in the ATF interface.
        if ((*iter) == "unprivileged-user" || (*iter) == "unprivileged_user")
            property = "unprivileged_user";
        else
            property = F("test_suites.%s.%s") % test_suite_name % (*iter);

        if (!user_config.is_set(property))
            return F("Required configuration property '%s' not defined") %
                (*iter);
    }
    return "";
}


/// Checks if the allowed architectures match the current architecture.
///
/// \param allowed_architectures Set of allowed architectures.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current architecture is in the list or an error
/// message otherwise.
static std::string
check_allowed_architectures(const engine::strings_set& allowed_architectures,
                            const config::tree& user_config)
{
    if (!allowed_architectures.empty()) {
        const std::string architecture =
            user_config.lookup< config::string_node >("architecture");
        if (allowed_architectures.find(architecture) ==
            allowed_architectures.end())
            return F("Current architecture '%s' not supported") % architecture;
    }
    return "";
}


/// Checks if the allowed platforms match the current architecture.
///
/// \param allowed_platforms Set of allowed platforms.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current platform is in the list or an error message
/// otherwise.
static std::string
check_allowed_platforms(const engine::strings_set& allowed_platforms,
                        const config::tree& user_config)
{
    if (!allowed_platforms.empty()) {
        const std::string platform =
            user_config.lookup< config::string_node >("platform");
        if (allowed_platforms.find(platform) == allowed_platforms.end())
            return F("Current platform '%s' not supported") % platform;
    }
    return "";
}


/// Checks if the current user matches the required user.
///
/// \param required_user Name of the required user category.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current user fits the required user characteristics or
/// an error message otherwise.
static std::string
check_required_user(const std::string& required_user,
                    const config::tree& user_config)
{
    if (!required_user.empty()) {
        const passwd::user user = passwd::current_user();
        if (required_user == "root") {
            if (!user.is_root())
                return "Requires root privileges";
        } else if (required_user == "unprivileged") {
            if (user.is_root())
                if (!user_config.is_set("unprivileged_user"))
                    return "Requires an unprivileged user but the "
                        "unprivileged-user configuration variable is not "
                        "defined";
        } else
            UNREACHABLE_MSG("Value of require.user not properly validated");
    }
    return "";
}


/// Checks if all required files exist.
///
/// \param required_files Set of paths.
///
/// \return Empty if the required files all exist or an error message otherwise.
static std::string
check_required_files(const engine::paths_set& required_files)
{
    for (engine::paths_set::const_iterator iter = required_files.begin();
         iter != required_files.end(); iter++) {
        INV((*iter).is_absolute());
        if (!fs::exists(*iter))
            return F("Required file '%s' not found") % *iter;
    }
    return "";
}


/// Checks if all required programs exist.
///
/// \param required_programs Set of paths.
///
/// \return Empty if the required programs all exist or an error message
/// otherwise.
static std::string
check_required_programs(const engine::paths_set& required_programs)
{
    for (engine::paths_set::const_iterator iter = required_programs.begin();
         iter != required_programs.end(); iter++) {
        if ((*iter).is_absolute()) {
            if (!fs::exists(*iter))
                return F("Required program '%s' not found") % *iter;
        } else {
            if (!fs::find_in_path((*iter).c_str()))
                return F("Required program '%s' not found in PATH") % *iter;
        }
    }
    return "";
}


/// Checks if the current system has the specified amount of memory.
///
/// \param required_memory Amount of required physical memory, or zero if not
///     applicable.
///
/// \return Empty if the current system has the required amount of memory or an
/// error message otherwise.
static std::string
check_required_memory(const units::bytes& required_memory)
{
    if (required_memory > 0) {
        const units::bytes physical_memory = utils::physical_memory();
        if (physical_memory > 0 && physical_memory < required_memory)
            return F("Requires %s bytes of physical memory but only %s "
                     "available") %
                required_memory.format() % physical_memory.format();
    }
    return "";
}


}  // anonymous namespace


/// Internal implementation of the metadata class.
struct engine::metadata::impl {
    /// Metadata properties.
    config::tree props;

    /// Constructor.
    ///
    /// \param props_ Metadata properties of the test.
    impl(const utils::config::tree& props_) :
        props(props_)
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
        return props == other.props;
    }
};


/// Constructor.
///
/// \param props Metadata properties of the test.
engine::metadata::metadata(const utils::config::tree& props) :
    _pimpl(new impl(props))
{
}


/// Destructor.
engine::metadata::~metadata(void)
{
}


/// Returns the architectures allowed by the test.
///
/// \return Set of architectures, or empty if this does not apply.
const engine::strings_set&
engine::metadata::allowed_architectures(void) const
{
    return _pimpl->props.lookup< config::strings_set_node >(
        "allowed_architectures");
}


/// Returns the platforms allowed by the test.
///
/// \return Set of platforms, or empty if this does not apply.
const engine::strings_set&
engine::metadata::allowed_platforms(void) const
{
    return _pimpl->props.lookup< config::strings_set_node >("allowed_platforms");
}


/// Returns all the user-defined metadata properties.
///
/// \return A key/value map of properties.
engine::properties_map
engine::metadata::custom(void) const
{
    return _pimpl->props.all_properties("custom", true);
}


/// Returns the description of the test.
///
/// \return Textual description; may be empty.
const std::string&
engine::metadata::description(void) const
{
    return _pimpl->props.lookup< config::string_node >("description");
}


/// Returns whether the test has a cleanup part or not.
///
/// \return True if there is a cleanup part; false otherwise.
bool
engine::metadata::has_cleanup(void) const
{
    return _pimpl->props.lookup< config::bool_node >("has_cleanup");
}


/// Returns the list of configuration variables needed by the test.
///
/// \return Set of configuration variables.
const engine::strings_set&
engine::metadata::required_configs(void) const
{
    return _pimpl->props.lookup< config::strings_set_node >("required_configs");
}


/// Returns the list of files needed by the test.
///
/// \return Set of paths.
const engine::paths_set&
engine::metadata::required_files(void) const
{
    return _pimpl->props.lookup< paths_set_node >("required_files");
}


/// Returns the amount of memory required by the test.
///
/// \return Number of bytes, or 0 if this does not apply.
const units::bytes&
engine::metadata::required_memory(void) const
{
    return _pimpl->props.lookup< bytes_node >("required_memory");
}


/// Returns the list of programs needed by the test.
///
/// \return Set of paths.
const engine::paths_set&
engine::metadata::required_programs(void) const
{
    return _pimpl->props.lookup< paths_set_node >("required_programs");
}


/// Returns the user required by the test.
///
/// \return One of unprivileged, root or empty.
const std::string&
engine::metadata::required_user(void) const
{
    return _pimpl->props.lookup< user_node >("required_user");
}


/// Returns the timeout of the test.
///
/// \return A time delta; should be compared to default_timeout to see if it has
/// been overriden.
const datetime::delta&
engine::metadata::timeout(void) const
{
    return _pimpl->props.lookup< delta_node >("timeout");
}


/// Externalizes the metadata to a set of key/value textual pairs.
///
/// \return A key/value representation of the metadata.
engine::properties_map
engine::metadata::to_properties(void) const
{
    return _pimpl->props.all_properties();
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
engine::metadata::operator==(const metadata& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
engine::metadata::operator!=(const metadata& other) const
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
engine::operator<<(std::ostream& output, const metadata& object)
{
    output << "metadata{";

    bool first = true;
    const engine::properties_map props = object.to_properties();
    for (engine::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        if (!first)
            output << ", ";
        output << F("%s=%s") % (*iter).first %
            text::quote((*iter).second, '\'');
        first = false;
    }

    output << "}";
    return output;
}


/// Internal implementation of the metadata_builder class.
struct engine::metadata_builder::impl {
    /// Collection of requirements.
    config::tree props;

    /// Whether we have created a metadata object or not.
    bool built;

    /// Constructor.
    impl(void) :
        built(false)
    {
        init_tree(props);
    }

    /// Constructor.
    impl(const engine::metadata& base) :
        props(base._pimpl->props.deep_copy()),
        built(false)
    {
    }
};


/// Constructor.
engine::metadata_builder::metadata_builder(void) :
    _pimpl(new impl())
{
}


/// Constructor.
engine::metadata_builder::metadata_builder(const engine::metadata& base) :
    _pimpl(new impl(base))
{
}


/// Destructor.
engine::metadata_builder::~metadata_builder(void)
{
}


/// Accumulates an additional allowed architecture.
///
/// \param arch The architecture.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_allowed_architecture(const std::string& arch)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "allowed_architectures").insert(arch);
    return *this;
}


/// Accumulates an additional allowed platform.
///
/// \param platform The platform.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_allowed_platform(const std::string& platform)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "allowed_platforms").insert(platform);
    return *this;
}


/// Accumulates a single user-defined property.
///
/// \param key Name of the property to define.
/// \param value Value of the property.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_custom(const std::string& key,
                                     const std::string& value)
{
    _pimpl->props.set_string(F("custom.%s") % key, value);
    return *this;
}


/// Accumulates an additional required configuration variable.
///
/// \param var The name of the configuration variable.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_config(const std::string& var)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "required_configs").insert(var);
    return *this;
}


/// Accumulates an additional required file.
///
/// \param path The path to the file.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_file(const fs::path& path)
{
    lookup_rw< paths_set_node >(_pimpl->props, "required_files").insert(path);
    return *this;
}


/// Accumulates an additional required program.
///
/// \param path The path to the program.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_program(const fs::path& path)
{
    lookup_rw< paths_set_node >(_pimpl->props,
                                "required_programs").insert(path);
    return *this;
}


/// Sets the architectures allowed by the test.
///
/// \param as Set of architectures.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_allowed_architectures(const strings_set& as)
{
    set< config::strings_set_node >(_pimpl->props, "allowed_architectures", as);
    return *this;
}


/// Sets the platforms allowed by the test.
///
/// \return ps Set of platforms.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_allowed_platforms(const strings_set& ps)
{
    set< config::strings_set_node >(_pimpl->props, "allowed_platforms", ps);
    return *this;
}


/// Sets the user-defined properties.
///
/// \param props The custom properties to set.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_custom(const properties_map& props)
{
    for (properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter)
        _pimpl->props.set_string(F("custom.%s") % (*iter).first,
                                 (*iter).second);
    return *this;
}


/// Sets the description of the test.
///
/// \param description Textual description of the test.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_description(const std::string& description)
{
    set< config::string_node >(_pimpl->props, "description", description);
    return *this;
}


/// Sets whether the test has a cleanup part or not.
///
/// \param cleanup True if the test has a cleanup part; false otherwise.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_has_cleanup(const bool cleanup)
{
    set< config::bool_node >(_pimpl->props, "has_cleanup", cleanup);
    return *this;
}


/// Sets the list of configuration variables needed by the test.
///
/// \param vars Set of configuration variables.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_configs(const strings_set& vars)
{
    set< config::strings_set_node >(_pimpl->props, "required_configs", vars);
    return *this;
}


/// Sets the list of files needed by the test.
///
/// \param files Set of paths.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_files(const paths_set& files)
{
    set< paths_set_node >(_pimpl->props, "required_files", files);
    return *this;
}


/// Sets the amount of memory required by the test.
///
/// \param bytes Number of bytes.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_memory(const units::bytes& bytes)
{
    set< bytes_node >(_pimpl->props, "required_memory", bytes);
    return *this;
}


/// Sets the list of programs needed by the test.
///
/// \param progs Set of paths.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_programs(const paths_set& progs)
{
    set< paths_set_node >(_pimpl->props, "required_programs", progs);
    return *this;
}


/// Sets the user required by the test.
///
/// \param user One of unprivileged, root or empty.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_user(const std::string& user)
{
    set< user_node >(_pimpl->props, "required_user", user);
    return *this;
}


/// Sets a metadata property by name from its textual representation.
///
/// \param key The property to set.
/// \param value The value to set the property to.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid or the key does not exist.
engine::metadata_builder&
engine::metadata_builder::set_string(const std::string& key,
                                     const std::string& value)
{
    try {
        _pimpl->props.set_string(key, value);
    } catch (const config::unknown_key_error& e) {
        throw engine::format_error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::format_error(
            F("Invalid value for metadata property %s: %s") % key % e.what());
    }
    return *this;
}


/// Sets the timeout of the test.
///
/// \param timeout The timeout to set.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_timeout(const datetime::delta& timeout)
{
    set< delta_node >(_pimpl->props, "timeout", timeout);
    return *this;
}


/// Creates a new metadata object.
///
/// \pre This has not yet been called.  We only support calling this function
/// once due to the way the internal tree works: we pass around references, not
/// deep copies, so if we allowed a second build, we'd encourage reusing the
/// same builder to construct different metadata objects, and this could have
/// unintended consequences.
///
/// \return The constructed metadata object.
engine::metadata
engine::metadata_builder::build(void) const
{
    PRE(!_pimpl->built);
    _pimpl->built = true;

    return metadata(_pimpl->props);
}


/// Checks if all the requirements specified by the test case are met.
///
/// \param md The test metadata.
/// \param cfg The engine configuration.
/// \param test_suite Name of the test suite the test belongs to.
///
/// \return A string describing the reason for skipping the test, or empty if
/// the test should be executed.
std::string
engine::check_reqs(const engine::metadata& md, const config::tree& cfg,
                   const std::string& test_suite)
{
    std::string reason;

    reason = check_required_configs(md.required_configs(), cfg, test_suite);
    if (!reason.empty())
        return reason;

    reason = check_allowed_architectures(md.allowed_architectures(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_allowed_platforms(md.allowed_platforms(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_required_user(md.required_user(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_required_files(md.required_files());
    if (!reason.empty())
        return reason;

    reason = check_required_programs(md.required_programs());
    if (!reason.empty())
        return reason;

    reason = check_required_memory(md.required_memory());
    if (!reason.empty())
        return reason;

    INV(reason.empty());
    return reason;
}
