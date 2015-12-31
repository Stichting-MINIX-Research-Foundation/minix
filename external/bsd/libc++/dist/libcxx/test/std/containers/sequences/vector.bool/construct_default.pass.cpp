//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <vector>
// vector<bool>

// vector(const Alloc& = Alloc());

#include <vector>
#include <cassert>

#include "test_macros.h"
#include "test_allocator.h"
#include "min_allocator.h"

template <class C>
void
test0()
{
#if TEST_STD_VER > 14
	static_assert((noexcept(C{})), "" );
#elif TEST_STD_VER >= 11
	static_assert((noexcept(C()) == noexcept(typename C::allocator_type())), "" );
#endif
    C c;
    assert(c.__invariants());
    assert(c.empty());
    assert(c.get_allocator() == typename C::allocator_type());
#if TEST_STD_VER >= 11
    C c1 = {};
    assert(c1.__invariants());
    assert(c1.empty());
    assert(c1.get_allocator() == typename C::allocator_type());
#endif
}

template <class C>
void
test1(const typename C::allocator_type& a)
{
#if TEST_STD_VER > 14
	static_assert((noexcept(C{typename C::allocator_type{}})), "" );
#elif TEST_STD_VER >= 11
	static_assert((noexcept(C(typename C::allocator_type())) == std::is_nothrow_copy_constructible<typename C::allocator_type>::value), "" );
#endif
    C c(a);
    assert(c.__invariants());
    assert(c.empty());
    assert(c.get_allocator() == a);
}

int main()
{
    {
    test0<std::vector<bool> >();
    test1<std::vector<bool, test_allocator<bool> > >(test_allocator<bool>(3));
    }
#if TEST_STD_VER >= 11
    {
    test0<std::vector<bool, min_allocator<bool>> >();
    test1<std::vector<bool, min_allocator<bool> > >(min_allocator<bool>());
    }
#endif
}
