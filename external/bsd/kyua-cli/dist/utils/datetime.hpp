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

/// \file utils/datetime.hpp
/// Provides date and time-related classes and functions.

#if !defined(UTILS_DATETIME_HPP)
#define UTILS_DATETIME_HPP

extern "C" {
#include <stdint.h>
}

#include <cstddef>
#include <ostream>
#include <string>

#include <tr1/memory>

namespace utils {
namespace datetime {


/// Represents a time delta to describe deadlines.
struct delta {
    /// The amount of seconds in the time delta.
    int64_t seconds;

    /// The amount of microseconds in the time delta.
    unsigned long useconds;

    delta(void);
    delta(const int64_t, const unsigned long);

    static delta from_microseconds(const int64_t);
    int64_t to_microseconds(void) const;

    bool operator==(const delta&) const;
    bool operator!=(const delta&) const;

    delta operator+(const delta&) const;
    delta operator+=(const delta&);
};


std::ostream& operator<<(std::ostream&, const delta&);


/// Represents a fixed date/time.
///
/// Timestamps are immutable objects and therefore we can simply use a shared
/// pointer to hide the implementation type of the date.  By not using an auto
/// pointer, we don't have to worry about providing our own copy constructor and
/// assignment opertor.
class timestamp {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::tr1::shared_ptr< impl > _pimpl;

    timestamp(std::tr1::shared_ptr< impl >);

public:
    static timestamp from_microseconds(const int64_t);
    static timestamp from_values(const int, const int, const int,
                                 const int, const int, const int,
                                 const int);
    static timestamp now(void);

    std::string strftime(const std::string&) const;
    int64_t to_microseconds(void) const;
    int64_t to_seconds(void) const;

    bool operator==(const timestamp&) const;
    bool operator!=(const timestamp&) const;

    delta operator-(const timestamp&) const;
};


std::ostream& operator<<(std::ostream&, const timestamp&);


void set_mock_now(const int, const int, const int, const int, const int,
                  const int, const int);


}  // namespace datetime
}  // namespace utils

#endif // !defined(UTILS_DATETIME_HPP)
