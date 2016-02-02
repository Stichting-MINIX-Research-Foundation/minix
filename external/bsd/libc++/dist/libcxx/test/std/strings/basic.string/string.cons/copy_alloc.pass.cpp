//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string(const basic_string& str, const Allocator& alloc);

#include <string>
#include <cassert>

#include "test_allocator.h"
#include "min_allocator.h"

template <class S>
void
test(S s1, const typename S::allocator_type& a)
{
    S s2(s1, a);
    assert(s2.__invariants());
    assert(s2 == s1);
    assert(s2.capacity() >= s2.size());
    assert(s2.get_allocator() == a);
}

int main()
{
    {
    typedef test_allocator<char> A;
    typedef std::basic_string<char, std::char_traits<char>, A> S;
    test(S(), A(3));
    test(S("1"), A(5));
    test(S("1234567890123456789012345678901234567890123456789012345678901234567890"), A(7));
    }
#if __cplusplus >= 201103L
    {
    typedef min_allocator<char> A;
    typedef std::basic_string<char, std::char_traits<char>, A> S;
    test(S(), A());
    test(S("1"), A());
    test(S("1234567890123456789012345678901234567890123456789012345678901234567890"), A());
    }
#endif
}
