//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// void resize(size_type n, charT c);

#include <string>
#include <stdexcept>
#include <cassert>

#include "min_allocator.h"

template <class S>
void
test(S s, typename S::size_type n, typename S::value_type c, S expected)
{
    try
    {
        s.resize(n, c);
        assert(s.__invariants());
        assert(n <= s.max_size());
        assert(s == expected);
    }
    catch (std::length_error&)
    {
        assert(n > s.max_size());
    }
}

int main()
{
    {
    typedef std::string S;
    test(S(), 0, 'a', S());
    test(S(), 1, 'a', S("a"));
    test(S(), 10, 'a', S(10, 'a'));
    test(S(), 100, 'a', S(100, 'a'));
    test(S("12345"), 0, 'a', S());
    test(S("12345"), 2, 'a', S("12"));
    test(S("12345"), 5, 'a', S("12345"));
    test(S("12345"), 15, 'a', S("12345aaaaaaaaaa"));
    test(S("12345678901234567890123456789012345678901234567890"), 0, 'a', S());
    test(S("12345678901234567890123456789012345678901234567890"), 10, 'a',
         S("1234567890"));
    test(S("12345678901234567890123456789012345678901234567890"), 50, 'a',
         S("12345678901234567890123456789012345678901234567890"));
    test(S("12345678901234567890123456789012345678901234567890"), 60, 'a',
         S("12345678901234567890123456789012345678901234567890aaaaaaaaaa"));
    test(S(), S::npos, 'a', S("not going to happen"));
    }
#if __cplusplus >= 201103L
    {
    typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
    test(S(), 0, 'a', S());
    test(S(), 1, 'a', S("a"));
    test(S(), 10, 'a', S(10, 'a'));
    test(S(), 100, 'a', S(100, 'a'));
    test(S("12345"), 0, 'a', S());
    test(S("12345"), 2, 'a', S("12"));
    test(S("12345"), 5, 'a', S("12345"));
    test(S("12345"), 15, 'a', S("12345aaaaaaaaaa"));
    test(S("12345678901234567890123456789012345678901234567890"), 0, 'a', S());
    test(S("12345678901234567890123456789012345678901234567890"), 10, 'a',
         S("1234567890"));
    test(S("12345678901234567890123456789012345678901234567890"), 50, 'a',
         S("12345678901234567890123456789012345678901234567890"));
    test(S("12345678901234567890123456789012345678901234567890"), 60, 'a',
         S("12345678901234567890123456789012345678901234567890aaaaaaaaaa"));
    test(S(), S::npos, 'a', S("not going to happen"));
    }
#endif
}
