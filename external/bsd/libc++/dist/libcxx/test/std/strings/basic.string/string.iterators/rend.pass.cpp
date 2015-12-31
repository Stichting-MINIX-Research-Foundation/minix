//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

//       reverse_iterator rend();
// const_reverse_iterator rend() const;

#include <string>
#include <cassert>

#include "min_allocator.h"

template <class S>
void
test(S s)
{
    const S& cs = s;
    typename S::reverse_iterator e = s.rend();
    typename S::const_reverse_iterator ce = cs.rend();
    if (s.empty())
    {
        assert(e == s.rbegin());
        assert(ce == cs.rbegin());
    }
    assert(e - s.rbegin() == s.size());
    assert(ce - cs.rbegin() == cs.size());
}

int main()
{
    {
    typedef std::string S;
    test(S());
    test(S("123"));
    }
#if __cplusplus >= 201103L
    {
    typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
    test(S());
    test(S("123"));
    }
#endif
}
