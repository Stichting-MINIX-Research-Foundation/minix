//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string(basic_string&& str, const Allocator& alloc);

#include <string>
#include <cassert>

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES

#include "test_macros.h"
#include "test_allocator.h"
#include "min_allocator.h"


template <class S>
void
test(S s0, const typename S::allocator_type& a)
{
    S s1 = s0;
    S s2(std::move(s0), a);
    assert(s2.__invariants());
    assert(s0.__invariants());
    assert(s2 == s1);
    assert(s2.capacity() >= s2.size());
    assert(s2.get_allocator() == a);
}

#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
// #if _LIBCPP_STD_VER <= 14
//         _NOEXCEPT_(is_nothrow_move_constructible<allocator_type>::value);
// #else
//         _NOEXCEPT;
// #endif

int main()
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
    typedef test_allocator<char> A;
    typedef std::basic_string<char, std::char_traits<char>, A> S;
#if TEST_STD_VER > 14
	static_assert((noexcept(S{})), "" );
#elif TEST_STD_VER >= 11
	static_assert((noexcept(S()) == std::is_nothrow_move_constructible<A>::value), "" );
#endif
    test(S(), A(3));
    test(S("1"), A(5));
    test(S("1234567890123456789012345678901234567890123456789012345678901234567890"), A(7));
    }

    int alloc_count = test_alloc_base::alloc_count;
    {
    typedef test_allocator<char> A;
    typedef std::basic_string<char, std::char_traits<char>, A> S;
#if TEST_STD_VER > 14
	static_assert((noexcept(S{})), "" );
#elif TEST_STD_VER >= 11
	static_assert((noexcept(S()) == std::is_nothrow_move_constructible<A>::value), "" );
#endif
    S s1 ( "Twas brillig, and the slivy toves did gyre and gymbal in the wabe" );
    S s2 (std::move(s1), A(1));
    }
    assert ( test_alloc_base::alloc_count == alloc_count );
    
#if TEST_STD_VER >= 11
    {
    typedef min_allocator<char> A;
    typedef std::basic_string<char, std::char_traits<char>, A> S;
#if TEST_STD_VER > 14
	static_assert((noexcept(S{})), "" );
#elif TEST_STD_VER >= 11
	static_assert((noexcept(S()) == std::is_nothrow_move_constructible<A>::value), "" );
#endif
    test(S(), A());
    test(S("1"), A());
    test(S("1234567890123456789012345678901234567890123456789012345678901234567890"), A());
    }
#endif
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
