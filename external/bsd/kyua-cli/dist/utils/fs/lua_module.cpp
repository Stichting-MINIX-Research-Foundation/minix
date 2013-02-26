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

#include "utils/fs/lua_module.hpp"

extern "C" {
#include <dirent.h>
}

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <lutok/operations.hpp>
#include <lutok/state.ipp>

#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;


namespace {


/// Lua binding for fs::path::basename.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) The basename of the input path.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_basename(lutok::state& state)
{
    if (!state.is_string())
        throw std::runtime_error("Need a string parameter");
    const fs::path path(state.to_string());

    state.push_string(path.leaf_name().c_str());
    return 1;
}


/// Lua binding for fs::path::dirname.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) The directory part of the input path.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_dirname(lutok::state& state)
{
    if (!state.is_string())
        throw std::runtime_error("Need a string parameter");
    const fs::path path(state.to_string());

    state.push_string(path.branch_path().c_str());
    return 1;
}


/// Lua binding for fs::path::exists.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) Whether the input path exists or not.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_exists(lutok::state& state)
{
    if (!state.is_string())
        throw std::runtime_error("Need a string parameter");
    const fs::path path(state.to_string());

    state.push_boolean(fs::exists(path));
    return 1;
}


/// Lua binding for the files iterator.
///
/// This function takes an open directory from the closure of the iterator and
/// returns the next entry.  See lua_fs_files() for the iterator generator
/// function.
///
/// \pre upvalue(1) The userdata containing an open DIR* object.
///
/// \param state The lua state.
///
/// \return The number of result values, i.e. 0 if there are no more entries or
/// 1 if an entry has been read.
static int
files_iterator(lutok::state& state)
{
    DIR** dirp = state.to_userdata< DIR* >(state.upvalue_index(1));
    const struct dirent* entry = ::readdir(*dirp);
    if (entry == NULL)
        return 0;
    else {
        state.push_string(entry->d_name);
        return 1;
    }
}


/// Lua binding for the destruction of the files iterator.
///
/// This function takes an open directory and closes it.  See lua_fs_files() for
/// the iterator generator function.
///
/// \pre stack(-1) The userdata containing an open DIR* object.
/// \post The DIR* object is closed.
///
/// \param state The lua state.
///
/// \return The number of result values, i.e. 0.
static int
files_gc(lutok::state& state)
{
    PRE(state.is_userdata());

    DIR** dirp = state.to_userdata< DIR* >();
    // For some reason, this may be called more than once.  I don't know why
    // this happens, but we must protect against it.
    if (*dirp != NULL) {
        ::closedir(*dirp);
        *dirp = NULL;
    }

    return 0;
}


/// Lua binding to create an iterator to scan the contents of a directory.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) The iterator function.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_files(lutok::state& state)
{
    if (!state.is_string())
        throw std::runtime_error("Need a string parameter");
    const fs::path path(state.to_string());

    DIR** dirp = state.new_userdata< DIR* >();

    state.new_table();
    state.push_string("__gc");
    state.push_cxx_function(files_gc);
    state.set_table();

    state.set_metatable();

    *dirp = ::opendir(path.c_str());
    if (*dirp == NULL) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to open directory: %s") %
                                 std::strerror(original_errno));
    }

    state.push_cxx_closure(files_iterator, 1);

    return 1;
}


/// Lua binding for fs::path::is_absolute.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) Whether the input path is absolute or not.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_is_absolute(lutok::state& state)
{
    if (!state.is_string())
        throw std::runtime_error("Need a string parameter");
    const fs::path path(state.to_string());

    state.push_boolean(path.is_absolute());
    return 1;
}


/// Lua binding for fs::path::operator/.
///
/// \pre stack(-2) The first input path.
/// \pre stack(-1) The second input path.
/// \post stack(-1) The concatenation of the two paths.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
static int
lua_fs_join(lutok::state& state)
{
    if (!state.is_string(-2))
        throw std::runtime_error("Need a string parameter");
    const fs::path path1(state.to_string(-2));

    if (!state.is_string(-1))
        throw std::runtime_error("Need a string parameter");
    const fs::path path2(state.to_string(-1));

    state.push_string((path1 / path2).c_str());
    return 1;
}


}  // anonymous namespace


/// Creates a Lua 'fs' module.
///
/// \post The global 'fs' symbol is set to a table that contains functions to a
/// variety of utilites from the fs C++ module.
///
/// \param s The Lua state.
void
fs::open_fs(lutok::state& s)
{
    std::map< std::string, lutok::cxx_function > members;
    members["basename"] = lua_fs_basename;
    members["dirname"] = lua_fs_dirname;
    members["exists"] = lua_fs_exists;
    members["files"] = lua_fs_files;
    members["is_absolute"] = lua_fs_is_absolute;
    members["join"] = lua_fs_join;
    lutok::create_module(s, "fs", members);
}
