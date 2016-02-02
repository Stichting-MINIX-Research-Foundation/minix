//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <queue>

// void swap(priority_queue& c)
//     noexcept(__is_nothrow_swappable<container_type>::value &&
//              __is_nothrow_swappable<Compare>::value);

// This tests a conforming extension

#include <queue>
#include <cassert>

#include "MoveOnly.h"

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef std::priority_queue<MoveOnly> C;
        C c1, c2;
        static_assert(noexcept(swap(c1, c2)), "");
    }
#endif
}
