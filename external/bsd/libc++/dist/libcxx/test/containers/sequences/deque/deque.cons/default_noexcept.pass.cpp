//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <deque>

// deque()
//        noexcept(is_nothrow_default_constructible<allocator_type>::value);

// This tests a conforming extension

#include <deque>
#include <cassert>

#include "../../../MoveOnly.h"
#include "test_allocator.h"

template <class T>
struct some_alloc
{
    typedef T value_type;
    some_alloc(const some_alloc&);
};

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef std::deque<MoveOnly> C;
        static_assert(std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::deque<MoveOnly, test_allocator<MoveOnly>> C;
        static_assert(std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::deque<MoveOnly, other_allocator<MoveOnly>> C;
        static_assert(!std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::deque<MoveOnly, some_alloc<MoveOnly>> C;
        static_assert(!std::is_nothrow_default_constructible<C>::value, "");
    }
#endif
}
