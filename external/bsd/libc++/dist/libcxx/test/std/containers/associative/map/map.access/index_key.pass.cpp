//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <map>

// class map

// mapped_type& operator[](const key_type& k);

#include <map>
#include <cassert>

#include "min_allocator.h"
#include "private_constructor.hpp"

int main()
{
    {
    typedef std::pair<const int, double> V;
    V ar[] =
    {
        V(1, 1.5),
        V(2, 2.5),
        V(3, 3.5),
        V(4, 4.5),
        V(5, 5.5),
        V(7, 7.5),
        V(8, 8.5),
    };
    std::map<int, double> m(ar, ar+sizeof(ar)/sizeof(ar[0]));
    assert(m.size() == 7);
    assert(m[1] == 1.5);
    assert(m.size() == 7);
    m[1] = -1.5;
    assert(m[1] == -1.5);
    assert(m.size() == 7);
    assert(m[6] == 0);
    assert(m.size() == 8);
    m[6] = 6.5;
    assert(m[6] == 6.5);
    assert(m.size() == 8);
    }
#if __cplusplus >= 201103L
    {
    typedef std::pair<const int, double> V;
    V ar[] =
    {
        V(1, 1.5),
        V(2, 2.5),
        V(3, 3.5),
        V(4, 4.5),
        V(5, 5.5),
        V(7, 7.5),
        V(8, 8.5),
    };
    std::map<int, double, std::less<int>, min_allocator<V>> m(ar, ar+sizeof(ar)/sizeof(ar[0]));
    assert(m.size() == 7);
    assert(m[1] == 1.5);
    assert(m.size() == 7);
    const int i = 1;
    m[i] = -1.5;
    assert(m[1] == -1.5);
    assert(m.size() == 7);
    assert(m[6] == 0);
    assert(m.size() == 8);
    m[6] = 6.5;
    assert(m[6] == 6.5);
    assert(m.size() == 8);
    }
#endif
#if _LIBCPP_STD_VER > 11
    {
    typedef std::pair<const int, double> V;
    V ar[] =
    {
        V(1, 1.5),
        V(2, 2.5),
        V(3, 3.5),
        V(4, 4.5),
        V(5, 5.5),
        V(7, 7.5),
        V(8, 8.5),
    };
    std::map<int, double, std::less<>> m(ar, ar+sizeof(ar)/sizeof(ar[0]));

    assert(m.size() == 7);
    assert(m[1] == 1.5);
    assert(m.size() == 7);
    m[1] = -1.5;
    assert(m[1] == -1.5);
    assert(m.size() == 7);
    assert(m[6] == 0);
    assert(m.size() == 8);
    m[6] = 6.5;
    assert(m[6] == 6.5);
    assert(m.size() == 8);
    }
#endif
}
