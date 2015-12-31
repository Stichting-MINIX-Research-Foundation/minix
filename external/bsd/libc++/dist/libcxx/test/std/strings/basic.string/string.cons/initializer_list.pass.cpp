//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string(initializer_list<charT> il, const Allocator& a = Allocator());

#include <string>
#include <cassert>

#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    {
        std::string s = {'a', 'b', 'c'};
        assert(s == "abc");
    }
    {
        std::wstring s;
        s = {L'a', L'b', L'c'};
        assert(s == L"abc");
    }
#if __cplusplus >= 201103L
    {
        typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
        S s = {'a', 'b', 'c'};
        assert(s == "abc");
    }
    {
        typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, min_allocator<wchar_t>> S;
        S s;
        s = {L'a', L'b', L'c'};
        assert(s == L"abc");
    }
#endif
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
