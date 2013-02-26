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

#include "utils/config/nodes.ipp"

#include <memory>

#include <lutok/state.ipp>

#include "utils/config/exceptions.hpp"
#include "utils/config/keys.hpp"
#include "utils/format/macros.hpp"

namespace config = utils::config;


/// Destructor.
config::detail::base_node::~base_node(void)
{
}


/// Constructor.
///
/// \param dynamic_ Whether the node is dynamic or not.
config::detail::inner_node::inner_node(const bool dynamic_) :
    _dynamic(dynamic_)
{
}


/// Destructor.
config::detail::inner_node::~inner_node(void)
{
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter)
        delete (*iter).second;
}


/// Fills the given node with a copy of this node's data.
///
/// \param node The node to fill.  Should be the fresh return value of a
///     deep_copy() operation.
void
config::detail::inner_node::copy_into(inner_node* node) const
{
    node->_dynamic = _dynamic;
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter) {
        base_node* new_node = (*iter).second->deep_copy();
        try {
            node->_children[(*iter).first] = new_node;
        } catch (...) {
            delete new_node;
            throw;
        }
    }
}


/// Finds a node without creating it if not found.
///
/// This recursive algorithm traverses the tree searching for a particular key.
/// The returned node is constant, so this can only be used for querying
/// purposes.  For this reason, this algorithm does not create intermediate
/// nodes if they don't exist (as would be necessary to set a new node).
///
/// \param key The key to be queried.
/// \param key_pos The current level within the key to be examined.
///
/// \return A reference to the located node, if successful.
///
/// \throw unknown_key_error If the provided key is unknown.
const config::detail::base_node*
config::detail::inner_node::lookup_ro(const tree_key& key,
                                      const tree_key::size_type key_pos) const
{
    PRE(key_pos < key.size());

    const children_map::const_iterator child_iter = _children.find(
        key[key_pos]);
    if (child_iter == _children.end())
        throw unknown_key_error(key);

    if (key_pos == key.size() - 1) {
        return (*child_iter).second;
    } else {
        PRE(key_pos < key.size() - 1);
        try {
            const inner_node& child = dynamic_cast< const inner_node& >(
                *(*child_iter).second);
            return child.lookup_ro(key, key_pos + 1);
        } catch (const std::bad_cast& e) {
            throw unknown_key_error(
                key, "Cannot address incomplete configuration property '%s'");
        }
    }
}


/// Finds a node and creates it if not found.
///
/// This recursive algorithm traverses the tree searching for a particular key,
/// creating any intermediate nodes if they do not already exist (for the case
/// of dynamic inner nodes).  The returned node is non-constant, so this can be
/// used by the algorithms that set key values.
///
/// \param key The key to be queried.
/// \param key_pos The current level within the key to be examined.
/// \param new_node A function that returns a new leaf node of the desired
///     type.  This is only called if the leaf cannot be found, but it has
///     already been defined.
///
/// \return A reference to the located node, if successful.
///
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the resulting node of the search would be an inner
///     node.
config::leaf_node*
config::detail::inner_node::lookup_rw(const tree_key& key,
                                      const tree_key::size_type key_pos,
                                      new_node_hook new_node)
{
    PRE(key_pos < key.size());

    children_map::const_iterator child_iter = _children.find(key[key_pos]);
    if (child_iter == _children.end()) {
        if (_dynamic) {
            base_node* const child = (key_pos == key.size() - 1) ?
                static_cast< base_node* >(new_node()) :
                static_cast< base_node* >(new dynamic_inner_node());
            _children.insert(children_map::value_type(key[key_pos], child));
            child_iter = _children.find(key[key_pos]);
        } else {
            throw unknown_key_error(key);
        }
    }

    if (key_pos == key.size() - 1) {
        try {
            leaf_node& child = dynamic_cast< leaf_node& >(
                *(*child_iter).second);
            return &child;
        } catch (const std::bad_cast& unused_error) {
            throw value_error(F("Invalid value for key '%s'") %
                              flatten_key(key));
        }
    } else {
        PRE(key_pos < key.size() - 1);
        try {
            inner_node& child = dynamic_cast< inner_node& >(
                *(*child_iter).second);
            return child.lookup_rw(key, key_pos + 1, new_node);
        } catch (const std::bad_cast& e) {
            throw unknown_key_error(
                key, "Cannot address incomplete configuration property '%s'");
        }
    }
}


/// Converts the subtree to a collection of key/value string pairs.
///
/// \param [out] properties The accumulator for the generated properties.  The
///     contents of the map are only extended.
/// \param key The path to the current node.
void
config::detail::inner_node::all_properties(properties_map& properties,
                                           const tree_key& key) const
{
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter) {
        tree_key child_key = key;
        child_key.push_back((*iter).first);
        try {
            leaf_node& child = dynamic_cast< leaf_node& >(*(*iter).second);
            if (child.is_set())
                properties[flatten_key(child_key)] = child.to_string();
        } catch (const std::bad_cast& unused_error) {
            inner_node& child = dynamic_cast< inner_node& >(*(*iter).second);
            child.all_properties(properties, child_key);
        }
    }
}


/// Constructor.
config::detail::static_inner_node::static_inner_node(void) :
    inner_node(false)
{
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::detail::static_inner_node::deep_copy(void) const
{
    std::auto_ptr< inner_node > new_node(new static_inner_node());
    copy_into(new_node.get());
    return new_node.release();
}


/// Registers a key as valid and having a specific type.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \param key The key to be registered.
/// \param key_pos The current level within the key to be examined.
/// \param new_node A function that returns a new leaf node of the desired
///     type.
void
config::detail::static_inner_node::define(const tree_key& key,
                                          const tree_key::size_type key_pos,
                                          new_node_hook new_node)
{
    PRE(key_pos < key.size());

    if (key_pos == key.size() - 1) {
        PRE_MSG(_children.find(key[key_pos]) == _children.end(),
                "Key already defined");
        _children.insert(children_map::value_type(key[key_pos], new_node()));
    } else {
        PRE(key_pos < key.size() - 1);
        const children_map::const_iterator child_iter = _children.find(
            key[key_pos]);

        if (child_iter == _children.end()) {
            static_inner_node* const child_ptr = new static_inner_node();
            _children.insert(children_map::value_type(key[key_pos], child_ptr));
            child_ptr->define(key, key_pos + 1, new_node);
        } else {
            try {
                static_inner_node& child = dynamic_cast< static_inner_node& >(
                    *(*child_iter).second);
                child.define(key, key_pos + 1, new_node);
            } catch (const std::bad_cast& e) {
                UNREACHABLE;
            }
        }
    }
}


/// Constructor.
config::detail::dynamic_inner_node::dynamic_inner_node(void) :
    inner_node(true)
{
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::detail::dynamic_inner_node::deep_copy(void) const
{
    std::auto_ptr< inner_node > new_node(new dynamic_inner_node());
    copy_into(new_node.get());
    return new_node.release();
}


/// Destructor.
config::leaf_node::~leaf_node(void)
{
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::bool_node::deep_copy(void) const
{
    std::auto_ptr< bool_node > new_node(new bool_node());
    new_node->_value = _value;
    return new_node.release();
}


/// Pushes the node's value onto the Lua stack.
///
/// \param state The Lua state onto which to push the value.
void
config::bool_node::push_lua(lutok::state& state) const
{
    state.push_boolean(value());
}


/// Sets the value of the node from an entry in the Lua stack.
///
/// \param state The Lua state from which to get the value.
/// \param value_index The stack index in which the value resides.
///
/// \throw value_error If the value in state(value_index) cannot be
///     processed by this node.
void
config::bool_node::set_lua(lutok::state& state, const int value_index)
{
    if (state.is_boolean(value_index))
        set(state.to_boolean(value_index));
    else
        throw value_error("Not a boolean");
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::int_node::deep_copy(void) const
{
    std::auto_ptr< int_node > new_node(new int_node());
    new_node->_value = _value;
    return new_node.release();
}


/// Pushes the node's value onto the Lua stack.
///
/// \param state The Lua state onto which to push the value.
void
config::int_node::push_lua(lutok::state& state) const
{
    state.push_integer(value());
}


/// Sets the value of the node from an entry in the Lua stack.
///
/// \param state The Lua state from which to get the value.
/// \param value_index The stack index in which the value resides.
///
/// \throw value_error If the value in state(value_index) cannot be
///     processed by this node.
void
config::int_node::set_lua(lutok::state& state, const int value_index)
{
    if (state.is_number(value_index))
        set(state.to_integer(value_index));
    else
        throw value_error("Not an integer");
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::string_node::deep_copy(void) const
{
    std::auto_ptr< string_node > new_node(new string_node());
    new_node->_value = _value;
    return new_node.release();
}


/// Pushes the node's value onto the Lua stack.
///
/// \param state The Lua state onto which to push the value.
void
config::string_node::push_lua(lutok::state& state) const
{
    state.push_string(value());
}


/// Sets the value of the node from an entry in the Lua stack.
///
/// \param state The Lua state from which to get the value.
/// \param value_index The stack index in which the value resides.
///
/// \throw value_error If the value in state(value_index) cannot be
///     processed by this node.
void
config::string_node::set_lua(lutok::state& state, const int value_index)
{
    if (state.is_string(value_index))
        set(state.to_string(value_index));
    else
        throw value_error("Not a string");
}


/// Copies the node.
///
/// \return A dynamically-allocated node.
config::detail::base_node*
config::strings_set_node::deep_copy(void) const
{
    std::auto_ptr< strings_set_node > new_node(new strings_set_node());
    new_node->_value = _value;
    return new_node.release();
}


/// Converts a single word to the native type.
///
/// \param raw_value The value to parse.
///
/// \return The parsed value.
std::string
config::strings_set_node::parse_one(const std::string& raw_value) const
{
    return raw_value;
}
