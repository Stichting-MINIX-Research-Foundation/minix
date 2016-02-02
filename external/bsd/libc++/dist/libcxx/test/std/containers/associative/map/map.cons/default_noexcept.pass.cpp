//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <map>

// map()
//    noexcept(
//        is_nothrow_default_constructible<allocator_type>::value &&
//        is_nothrow_default_constructible<key_compare>::value &&
//        is_nothrow_copy_constructible<key_compare>::value);

// This tests a conforming extension

#include <map>
#include <cassert>

#include "MoveOnly.h"
#include "test_allocator.h"

template <class T>
struct some_comp
{
    typedef T value_type;
    some_comp();
};

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef std::map<MoveOnly, MoveOnly> C;
        static_assert(std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, std::less<MoveOnly>, test_allocator<MoveOnly>> C;
        static_assert(std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, std::less<MoveOnly>, other_allocator<MoveOnly>> C;
        static_assert(!std::is_nothrow_default_constructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, some_comp<MoveOnly>> C;
        static_assert(!std::is_nothrow_default_constructible<C>::value, "");
    }
#endif
}
