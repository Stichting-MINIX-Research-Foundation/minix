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

/// \file utils/config/nodes.hpp
/// Representation of tree nodes.

#if !defined(UTILS_CONFIG_NODES_HPP)
#define UTILS_CONFIG_NODES_HPP

#include <map>
#include <set>
#include <string>

#include <lutok/state.hpp>

#include "utils/config/keys.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.hpp"

namespace utils {
namespace config {


/// Flat representation of all properties as strings.
typedef std::map< std::string, std::string > properties_map;


namespace detail {


/// Base representation of a node.
///
/// This abstract class provides the base type for every node in the tree.  Due
/// to the dynamic nature of our trees (each leaf being able to hold arbitrary
/// data types), this base type is a necessity.
class base_node : noncopyable {
public:
    virtual ~base_node(void) = 0;

    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node* deep_copy(void) const = 0;
};


class static_inner_node;


}  // namespace detail


/// Abstract leaf node without any specified type.
///
/// This base abstract type is necessary to have a common pointer type to which
/// to cast any leaf.  We later provide templated derivates of this class, and
/// those cannot act in this manner.
///
/// It is important to understand that a leaf can exist without actually holding
/// a value.  Our trees are "strictly keyed": keys must have been pre-defined
/// before a value can be set on them.  This is to ensure that the end user is
/// using valid key names and not making mistakes due to typos, for example.  To
/// represent this condition, we define an "empty" key in the tree to denote
/// that the key is valid, yet it has not been set by the user.  Only when an
/// explicit set is performed on the key, it gets a value.
class leaf_node : public detail::base_node {
public:
    virtual ~leaf_node(void);

    /// Checks whether the node has been set by the user.
    ///
    /// Nodes of the tree are predefined by the caller to specify the valid
    /// types of the leaves.  Such predefinition results in the creation of
    /// nodes within the tree, but these nodes have not yet been set.
    /// Traversing these nodes is invalid and should result in an "unknown key"
    /// error.
    ///
    /// \return True if a value has been set in the node.
    virtual bool is_set(void) const = 0;

    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param state The Lua state onto which to push the value.
    virtual void push_lua(lutok::state& state) const = 0;

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param state The Lua state from which to get the value.
    /// \param value_index The stack index in which the value resides.
    ///
    /// \throw value_error If the value in state(value_index) cannot be
    ///     processed by this node.
    virtual void set_lua(lutok::state& state, const int value_index) = 0;

    /// Sets the value of the node from a raw string representation.
    ///
    /// \param raw_value The value to set the node to.
    ///
    /// \throw value_error If the value is invalid.
    virtual void set_string(const std::string& raw_value) = 0;

    /// Converts the contents of the node to a string.
    ///
    /// \pre The node must have a value.
    ///
    /// \return A string representation of the value held by the node.
    virtual std::string to_string(void) const = 0;
};


/// Base leaf node for a single arbitrary type.
///
/// This templated leaf node holds a single object of any type.  The conversion
/// to/from string representations is undefined, as that depends on the
/// particular type being processed.  You should reimplement this class for any
/// type that needs additional processing/validation during conversion.
template< typename ValueType >
class typed_leaf_node : public leaf_node {
public:
    /// The type of the value held by this node.
    typedef ValueType value_type;

    typed_leaf_node(void);

    bool is_set(void) const;

    /// Gets the value stored in the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \pre The node must have a value.
    ///
    /// \return The value in the node.
    const value_type& value(void) const;

    /// Gets the read-write value stored in the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \pre The node must have a value.
    ///
    /// \return The value in the node.
    value_type& value(void);

    /// Sets the value of the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \param value_ The new value to set the node to.
    void set(const value_type&);

protected:
    /// The value held by this node.
    optional< value_type > _value;

private:
    virtual void validate(const value_type&) const;
};


/// Leaf node holding a native type.
///
/// This templated leaf node holds a native type.  The conversion to/from string
/// representations of the value happens by means of iostreams.
template< typename ValueType >
class native_leaf_node : public typed_leaf_node< ValueType > {
public:
    void set_string(const std::string&);
    std::string to_string(void) const;
};


/// A leaf node that holds a boolean value.
class bool_node : public native_leaf_node< bool > {
public:
    virtual base_node* deep_copy(void) const;

    void push_lua(lutok::state&) const;
    void set_lua(lutok::state&, const int);
};


/// A leaf node that holds an integer value.
class int_node : public native_leaf_node< int > {
public:
    virtual base_node* deep_copy(void) const;

    void push_lua(lutok::state&) const;
    void set_lua(lutok::state&, const int);
};


/// A leaf node that holds a string value.
class string_node : public native_leaf_node< std::string > {
public:
    virtual base_node* deep_copy(void) const;

    void push_lua(lutok::state&) const;
    void set_lua(lutok::state&, const int);
};


/// Base leaf node for a set of native types.
///
/// This is a base abstract class because there is no generic way to parse a
/// single word in the textual representation of the set to the native value.
template< typename ValueType >
class base_set_node : public leaf_node {
public:
    /// The type of the value held by this node.
    typedef std::set< ValueType > value_type;

    base_set_node(void);

    bool is_set(void) const;

    /// Gets the value stored in the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \pre The node must have a value.
    ///
    /// \return The value in the node.
    const value_type& value(void) const;

    /// Gets the read-write value stored in the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \pre The node must have a value.
    ///
    /// \return The value in the node.
    value_type& value(void);

    /// Sets the value of the node.
    ///
    /// \todo Figure out why Doxygen is unable to pick up the documentation for
    /// this function from the nodes.ipp file.
    ///
    /// \param value_ The new value to set the node to.
    void set(const value_type&);

    void set_string(const std::string&);
    std::string to_string(void) const;

    void push_lua(lutok::state&) const;
    void set_lua(lutok::state&, const int);

protected:
    /// The value held by this node.
    optional< value_type > _value;

private:
    /// Converts a single word to the native type.
    ///
    /// \param raw_value The value to parse.
    ///
    /// \return The parsed value.
    ///
    /// \throw value_error If the value is invalid.
    virtual ValueType parse_one(const std::string& raw_value) const = 0;

    virtual void validate(const value_type&) const;
};


/// A leaf node that holds a set of strings.
class strings_set_node : public base_set_node< std::string > {
public:
    virtual base_node* deep_copy(void) const;

private:
    std::string parse_one(const std::string&) const;
};


}  // namespace config
}  // namespace utils

#endif  // !defined(UTILS_CONFIG_NODES_HPP)
