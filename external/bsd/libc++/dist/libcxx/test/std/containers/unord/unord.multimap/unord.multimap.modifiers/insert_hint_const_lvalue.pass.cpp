//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_map>

// template <class Key, class T, class Hash = hash<Key>, class Pred = equal_to<Key>,
//           class Alloc = allocator<pair<const Key, T>>>
// class unordered_multimap

// iterator insert(const_iterator p, const value_type& x);

#if _LIBCPP_DEBUG >= 1
#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))
#endif

#include <unordered_map>
#include <cassert>

#include "min_allocator.h"

int main()
{
    {
        typedef std::unordered_multimap<double, int> C;
        typedef C::iterator R;
        typedef C::value_type P;
        C c;
        C::const_iterator e = c.end();
        R r = c.insert(e, P(3.5, 3));
        assert(c.size() == 1);
        assert(r->first == 3.5);
        assert(r->second == 3);

        r = c.insert(c.end(), P(3.5, 4));
        assert(c.size() == 2);
        assert(r->first == 3.5);
        assert(r->second == 4);

        r = c.insert(c.end(), P(4.5, 4));
        assert(c.size() == 3);
        assert(r->first == 4.5);
        assert(r->second == 4);

        r = c.insert(c.end(), P(5.5, 4));
        assert(c.size() == 4);
        assert(r->first == 5.5);
        assert(r->second == 4);
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_multimap<double, int, std::hash<double>, std::equal_to<double>,
                            min_allocator<std::pair<const double, int>>> C;
        typedef C::iterator R;
        typedef C::value_type P;
        C c;
        C::const_iterator e = c.end();
        R r = c.insert(e, P(3.5, 3));
        assert(c.size() == 1);
        assert(r->first == 3.5);
        assert(r->second == 3);

        r = c.insert(c.end(), P(3.5, 4));
        assert(c.size() == 2);
        assert(r->first == 3.5);
        assert(r->second == 4);

        r = c.insert(c.end(), P(4.5, 4));
        assert(c.size() == 3);
        assert(r->first == 4.5);
        assert(r->second == 4);

        r = c.insert(c.end(), P(5.5, 4));
        assert(c.size() == 4);
        assert(r->first == 5.5);
        assert(r->second == 4);
    }
#endif
#if _LIBCPP_DEBUG >= 1
    {
        typedef std::unordered_multimap<double, int> C;
        typedef C::iterator R;
        typedef C::value_type P;
        C c;
        C c2;
        C::const_iterator e = c2.end();
        P v(3.5, 3);
        R r = c.insert(e, v);
        assert(false);
    }
#endif
}
