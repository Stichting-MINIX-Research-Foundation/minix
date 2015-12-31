//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <map>

// ~map() // implied noexcept;

#include <map>
#include <cassert>

#include "../../../MoveOnly.h"
#include "test_allocator.h"

#if __has_feature(cxx_noexcept)

template <class T>
struct some_comp
{
    typedef T value_type;
    ~some_comp() noexcept(false);
};

#endif

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef std::map<MoveOnly, MoveOnly> C;
        static_assert(std::is_nothrow_destructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, std::less<MoveOnly>, test_allocator<MoveOnly>> C;
        static_assert(std::is_nothrow_destructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, std::less<MoveOnly>, other_allocator<MoveOnly>> C;
        static_assert(std::is_nothrow_destructible<C>::value, "");
    }
    {
        typedef std::map<MoveOnly, MoveOnly, some_comp<MoveOnly>> C;
        static_assert(!std::is_nothrow_destructible<C>::value, "");
    }
#endif
}
