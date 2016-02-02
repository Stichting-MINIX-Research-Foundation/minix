//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <vector>

// vector();
// vector(const Alloc&);

#include <vector>
#include <cassert>

#include "test_macros.h"
#include "test_allocator.h"
#include "../../../NotConstructible.h"
#include "../../../stack_allocator.h"
#include "min_allocator.h"
#include "asan_testing.h"

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
    assert(is_contiguous_container_asan_correct(c)); 
#if TEST_STD_VER >= 11
    C c1 = {};
    assert(c1.__invariants());
    assert(c1.empty());
    assert(c1.get_allocator() == typename C::allocator_type());
    assert(is_contiguous_container_asan_correct(c1)); 
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
    assert(is_contiguous_container_asan_correct(c)); 
}

int main()
{
    {
    test0<std::vector<int> >();
    test0<std::vector<NotConstructible> >();
    test1<std::vector<int, test_allocator<int> > >(test_allocator<int>(3));
    test1<std::vector<NotConstructible, test_allocator<NotConstructible> > >
        (test_allocator<NotConstructible>(5));
    }
    {
        std::vector<int, stack_allocator<int, 10> > v;
        assert(v.empty());
    }
#if TEST_STD_VER >= 11
    {
    test0<std::vector<int, min_allocator<int>> >();
    test0<std::vector<NotConstructible, min_allocator<NotConstructible>> >();
    test1<std::vector<int, min_allocator<int> > >(min_allocator<int>{});
    test1<std::vector<NotConstructible, min_allocator<NotConstructible> > >
        (min_allocator<NotConstructible>{});
    }
    {
        std::vector<int, min_allocator<int> > v;
        assert(v.empty());
    }
#endif
}
