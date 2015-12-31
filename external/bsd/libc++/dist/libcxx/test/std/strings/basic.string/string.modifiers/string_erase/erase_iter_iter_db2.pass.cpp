//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// Call erase(const_iterator first, const_iterator last); with second iterator from another container

#if _LIBCPP_DEBUG >= 1

#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))

#include <string>
#include <cassert>
#include <exception>
#include <cstdlib>

#include "min_allocator.h"

int main()
{
    {
    std::string l1("123");
    std::string l2("123");
    std::string::iterator i = l1.erase(l1.cbegin(), l2.cbegin()+1);
    assert(false);
    }
#if __cplusplus >= 201103L
    {
    typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
    S l1("123");
    S l2("123");
    S::iterator i = l1.erase(l1.cbegin(), l2.cbegin()+1);
    assert(false);
    }
#endif
}

#else

int main()
{
}

#endif
