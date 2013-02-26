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

#include "utils/stream.hpp"

#include <istream>
#include <sstream>

#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"


/// Gets the length of a stream.
///
/// \param is The input stream for which to calculate its length.
///
/// \return The length of the stream.  This is of size_t type instead of
/// directly std::streampos to simplify the caller.  Some systems do not
/// support comparing a std::streampos directly to an integer (see
/// NetBSD 1.5.x), which is what we often want to do.
///
/// \throw std::exception If calculating the length fails due to a stream error.
std::size_t
utils::stream_length(std::istream& is)
{
    const std::streampos current_pos = is.tellg();
    try {
        is.seekg(0, std::ios::end);
        const std::streampos length = is.tellg();
        is.seekg(current_pos, std::ios::beg);
        return static_cast< std::size_t >(length);
    } catch (...) {
        is.seekg(current_pos, std::ios::beg);
        throw;
    }
}


/// Reads the whole contents of a stream into memory.
///
/// \param input The input stream from which to read.
///
/// \return A plain string containing the raw contents of the file.
std::string
utils::read_stream(std::istream& input)
{
    std::ostringstream buffer;

    char tmp[1024];
    while (input.good()) {
        input.read(tmp, sizeof(tmp));
        if (input.good() || input.eof()) {
            buffer.write(tmp, input.gcount());
        }
    }

    return buffer.str();
}
