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

// map& operator=(const map& m);

#include <map>
#include <cassert>

#include "../../../test_compare.h"
#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
    {
        typedef std::pair<const int, double> V;
        V ar[] =
        {
            V(1, 1),
            V(1, 1.5),
            V(1, 2),
            V(2, 1),
            V(2, 1.5),
            V(2, 2),
            V(3, 1),
            V(3, 1.5),
            V(3, 2)
        };
        typedef test_compare<std::less<int> > C;
        typedef test_allocator<V> A;
        std::map<int, double, C, A> mo(ar, ar+sizeof(ar)/sizeof(ar[0]), C(5), A(2));
        std::map<int, double, C, A> m(ar, ar+sizeof(ar)/sizeof(ar[0])/2, C(3), A(7));
        m = mo;
        assert(m.get_allocator() == A(7));
        assert(m.key_comp() == C(5));
        assert(m.size() == 3);
        assert(distance(m.begin(), m.end()) == 3);
        assert(*m.begin() == V(1, 1));
        assert(*next(m.begin()) == V(2, 1));
        assert(*next(m.begin(), 2) == V(3, 1));

        assert(mo.get_allocator() == A(2));
        assert(mo.key_comp() == C(5));
        assert(mo.size() == 3);
        assert(distance(mo.begin(), mo.end()) == 3);
        assert(*mo.begin() == V(1, 1));
        assert(*next(mo.begin()) == V(2, 1));
        assert(*next(mo.begin(), 2) == V(3, 1));
    }
    {
        typedef std::pair<const int, double> V;
        const V ar[] =
        {
            V(1, 1),
            V(2, 1),
            V(3, 1),
        };
        std::map<int, double> m(ar, ar+sizeof(ar)/sizeof(ar[0]));
        std::map<int, double> *p = &m;
        m = *p;

        assert(m.size() == 3);
        assert(std::equal(m.begin(), m.end(), ar));
    }
    {
        typedef std::pair<const int, double> V;
        V ar[] =
        {
            V(1, 1),
            V(1, 1.5),
            V(1, 2),
            V(2, 1),
            V(2, 1.5),
            V(2, 2),
            V(3, 1),
            V(3, 1.5),
            V(3, 2)
        };
        typedef test_compare<std::less<int> > C;
        typedef other_allocator<V> A;
        std::map<int, double, C, A> mo(ar, ar+sizeof(ar)/sizeof(ar[0]), C(5), A(2));
        std::map<int, double, C, A> m(ar, ar+sizeof(ar)/sizeof(ar[0])/2, C(3), A(7));
        m = mo;
        assert(m.get_allocator() == A(2));
        assert(m.key_comp() == C(5));
        assert(m.size() == 3);
        assert(distance(m.begin(), m.end()) == 3);
        assert(*m.begin() == V(1, 1));
        assert(*next(m.begin()) == V(2, 1));
        assert(*next(m.begin(), 2) == V(3, 1));

        assert(mo.get_allocator() == A(2));
        assert(mo.key_comp() == C(5));
        assert(mo.size() == 3);
        assert(distance(mo.begin(), mo.end()) == 3);
        assert(*mo.begin() == V(1, 1));
        assert(*next(mo.begin()) == V(2, 1));
        assert(*next(mo.begin(), 2) == V(3, 1));
    }
#if __cplusplus >= 201103L
    {
        typedef std::pair<const int, double> V;
        V ar[] =
        {
            V(1, 1),
            V(1, 1.5),
            V(1, 2),
            V(2, 1),
            V(2, 1.5),
            V(2, 2),
            V(3, 1),
            V(3, 1.5),
            V(3, 2)
        };
        typedef test_compare<std::less<int> > C;
        typedef min_allocator<V> A;
        std::map<int, double, C, A> mo(ar, ar+sizeof(ar)/sizeof(ar[0]), C(5), A());
        std::map<int, double, C, A> m(ar, ar+sizeof(ar)/sizeof(ar[0])/2, C(3), A());
        m = mo;
        assert(m.get_allocator() == A());
        assert(m.key_comp() == C(5));
        assert(m.size() == 3);
        assert(distance(m.begin(), m.end()) == 3);
        assert(*m.begin() == V(1, 1));
        assert(*next(m.begin()) == V(2, 1));
        assert(*next(m.begin(), 2) == V(3, 1));

        assert(mo.get_allocator() == A());
        assert(mo.key_comp() == C(5));
        assert(mo.size() == 3);
        assert(distance(mo.begin(), mo.end()) == 3);
        assert(*mo.begin() == V(1, 1));
        assert(*next(mo.begin()) == V(2, 1));
        assert(*next(mo.begin(), 2) == V(3, 1));
    }
    {
        typedef std::pair<const int, double> V;
        V ar[] =
        {
            V(1, 1),
            V(1, 1.5),
            V(1, 2),
            V(2, 1),
            V(2, 1.5),
            V(2, 2),
            V(3, 1),
            V(3, 1.5),
            V(3, 2)
        };
        typedef test_compare<std::less<int> > C;
        typedef min_allocator<V> A;
        std::map<int, double, C, A> mo(ar, ar+sizeof(ar)/sizeof(ar[0]), C(5), A());
        std::map<int, double, C, A> m(ar, ar+sizeof(ar)/sizeof(ar[0])/2, C(3), A());
        m = mo;
        assert(m.get_allocator() == A());
        assert(m.key_comp() == C(5));
        assert(m.size() == 3);
        assert(distance(m.begin(), m.end()) == 3);
        assert(*m.begin() == V(1, 1));
        assert(*next(m.begin()) == V(2, 1));
        assert(*next(m.begin(), 2) == V(3, 1));

        assert(mo.get_allocator() == A());
        assert(mo.key_comp() == C(5));
        assert(mo.size() == 3);
        assert(distance(mo.begin(), mo.end()) == 3);
        assert(*mo.begin() == V(1, 1));
        assert(*next(mo.begin()) == V(2, 1));
        assert(*next(mo.begin(), 2) == V(3, 1));
    }
#endif
}
