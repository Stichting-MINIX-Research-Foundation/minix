//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <utility>

// template <class T1, class T2> struct pair

// pair(const pair&) = default;

#include <utility>
#include <cassert>

int main()
{
    {
        typedef std::pair<int, short> P1;
        P1 p1(3, 4);
        P1 p2 = p1;
        assert(p2.first == 3);
        assert(p2.second == 4);
    }

    static_assert((std::is_trivially_copy_constructible<std::pair<int, int> >::value), "");

#if _LIBCPP_STD_VER > 11
    {
        typedef std::pair<int, short> P1;
        constexpr P1 p1(3, 4);
        constexpr P1 p2 = p1;
        static_assert(p2.first == 3, "");
        static_assert(p2.second == 4, "");
    }
#endif
}
