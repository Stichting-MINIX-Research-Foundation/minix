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

// template <class U, class V> pair(const pair<U, V>& p);

#include <utility>
#include <cassert>

int main()
{
    {
        typedef std::pair<int, short> P1;
        typedef std::pair<double, long> P2;
        P1 p1(3, 4);
        P2 p2 = p1;
        assert(p2.first == 3);
        assert(p2.second == 4);
    }

#if _LIBCPP_STD_VER > 11
    {
        typedef std::pair<int, short> P1;
        typedef std::pair<double, long> P2;
        constexpr P1 p1(3, 4);
        constexpr P2 p2 = p1;
        static_assert(p2.first == 3, "");
        static_assert(p2.second == 4, "");
    }
#endif
}
