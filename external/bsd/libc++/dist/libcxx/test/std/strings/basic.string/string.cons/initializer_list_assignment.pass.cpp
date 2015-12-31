//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string& operator=(initializer_list<charT> il);

#include <string>
#include <cassert>

#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    {
        std::string s;
        s = {'a', 'b', 'c'};
        assert(s == "abc");
    }
#if __cplusplus >= 201103L
    {
        typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
        S s;
        s = {'a', 'b', 'c'};
        assert(s == "abc");
    }
#endif
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
