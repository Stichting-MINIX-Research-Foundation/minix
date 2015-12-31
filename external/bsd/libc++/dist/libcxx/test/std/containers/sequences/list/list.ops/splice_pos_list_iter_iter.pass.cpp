//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// void splice(const_iterator position, list& x, iterator first, iterator last);

#if _LIBCPP_DEBUG >= 1
#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))
#endif

#include <list>
#include <cassert>

#include "min_allocator.h"

int main()
{
    int a1[] = {1, 2, 3};
    int a2[] = {4, 5, 6};
    {
        std::list<int> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin()));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
    }
    {
        std::list<int> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin(), 2));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 2);
        ++i;
        assert(*i == 1);
        ++i;
        assert(*i == 3);
    }
    {
        std::list<int> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin(), 3));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        ++i;
        assert(*i == 1);
    }
    {
        std::list<int> l1(a1, a1+3);
        std::list<int> l2(a2, a2+3);
        l1.splice(l1.begin(), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        ++i;
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
    {
        std::list<int> l1(a1, a1+3);
        std::list<int> l2(a2, a2+3);
        l1.splice(next(l1.begin()), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
    {
        std::list<int> l1(a1, a1+3);
        std::list<int> l2(a2, a2+3);
        l1.splice(l1.end(), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        ++i;
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
#if _LIBCPP_DEBUG >= 1
    {
        std::list<int> v1(3);
        std::list<int> v2(3);
        v1.splice(v1.begin(), v2, v2.begin(), v1.end());
        assert(false);
    }
#endif
#if __cplusplus >= 201103L
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin()));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
    }
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin(), 2));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 2);
        ++i;
        assert(*i == 1);
        ++i;
        assert(*i == 3);
    }
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        l1.splice(l1.begin(), l1, next(l1.begin()), next(l1.begin(), 3));
        assert(l1.size() == 3);
        assert(distance(l1.begin(), l1.end()) == 3);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        ++i;
        assert(*i == 1);
    }
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        std::list<int, min_allocator<int>> l2(a2, a2+3);
        l1.splice(l1.begin(), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        ++i;
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        std::list<int, min_allocator<int>> l2(a2, a2+3);
        l1.splice(next(l1.begin()), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
    {
        std::list<int, min_allocator<int>> l1(a1, a1+3);
        std::list<int, min_allocator<int>> l2(a2, a2+3);
        l1.splice(l1.end(), l2, next(l2.begin()), l2.end());
        assert(l1.size() == 5);
        assert(distance(l1.begin(), l1.end()) == 5);
        std::list<int, min_allocator<int>>::const_iterator i = l1.begin();
        assert(*i == 1);
        ++i;
        assert(*i == 2);
        ++i;
        assert(*i == 3);
        ++i;
        assert(*i == 5);
        ++i;
        assert(*i == 6);
        assert(l2.size() == 1);
        i = l2.begin();
        assert(*i == 4);
    }
#if _LIBCPP_DEBUG >= 1
    {
        std::list<int, min_allocator<int>> v1(3);
        std::list<int, min_allocator<int>> v2(3);
        v1.splice(v1.begin(), v2, v2.begin(), v1.end());
        assert(false);
    }
#endif
#endif
}
