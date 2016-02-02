//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_map>

// Dereference non-dereferenceable iterator.

#if _LIBCPP_DEBUG >= 1

#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))

#include <unordered_map>
#include <string>
#include <cassert>
#include <iterator>
#include <exception>
#include <cstdlib>

#include "min_allocator.h"

int main()
{
    {
    typedef std::unordered_map<int, std::string> C;
    C c;
    c.insert(std::make_pair(1, "one"));
    C::iterator i = c.end();
    C::value_type j = *i;
    assert(false);
    }
#if __cplusplus >= 201103L
    {
    typedef std::unordered_map<int, std::string, std::hash<int>, std::equal_to<int>,
                        min_allocator<std::pair<const int, std::string>>> C;
    C c;
    c.insert(std::make_pair(1, "one"));
    C::iterator i = c.end();
    C::value_type j = *i;
    assert(false);
    }
#endif
}

#else

int main()
{
}

#endif
