//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <map>

// class multimap

//       iterator lower_bound(const key_type& k);
// const_iterator lower_bound(const key_type& k) const;

#include <map>
#include <cassert>

#include "min_allocator.h"
#include "private_constructor.hpp"

int main()
{
    typedef std::pair<const int, double> V;
    {
    typedef std::multimap<int, double> M;
    {
        typedef M::iterator R;
        V ar[] =
        {
            V(5, 1),
            V(5, 2),
            V(5, 3),
            V(7, 1),
            V(7, 2),
            V(7, 3),
            V(9, 1),
            V(9, 2),
            V(9, 3)
        };
        M m(ar, ar+sizeof(ar)/sizeof(ar[0]));
        R r = m.lower_bound(4);
        assert(r == m.begin());
        r = m.lower_bound(5);
        assert(r == m.begin());
        r = m.lower_bound(6);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(7);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(8);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(9);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(10);
        assert(r == m.end());
    }
    {
        typedef M::const_iterator R;
        V ar[] =
        {
            V(5, 1),
            V(5, 2),
            V(5, 3),
            V(7, 1),
            V(7, 2),
            V(7, 3),
            V(9, 1),
            V(9, 2),
            V(9, 3)
        };
        const M m(ar, ar+sizeof(ar)/sizeof(ar[0]));
        R r = m.lower_bound(4);
        assert(r == m.begin());
        r = m.lower_bound(5);
        assert(r == m.begin());
        r = m.lower_bound(6);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(7);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(8);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(9);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(10);
        assert(r == m.end());
    }
    }
#if __cplusplus >= 201103L
    {
    typedef std::multimap<int, double, std::less<int>, min_allocator<std::pair<const int, double>>> M;
    {
        typedef M::iterator R;
        V ar[] =
        {
            V(5, 1),
            V(5, 2),
            V(5, 3),
            V(7, 1),
            V(7, 2),
            V(7, 3),
            V(9, 1),
            V(9, 2),
            V(9, 3)
        };
        M m(ar, ar+sizeof(ar)/sizeof(ar[0]));
        R r = m.lower_bound(4);
        assert(r == m.begin());
        r = m.lower_bound(5);
        assert(r == m.begin());
        r = m.lower_bound(6);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(7);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(8);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(9);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(10);
        assert(r == m.end());
    }
    {
        typedef M::const_iterator R;
        V ar[] =
        {
            V(5, 1),
            V(5, 2),
            V(5, 3),
            V(7, 1),
            V(7, 2),
            V(7, 3),
            V(9, 1),
            V(9, 2),
            V(9, 3)
        };
        const M m(ar, ar+sizeof(ar)/sizeof(ar[0]));
        R r = m.lower_bound(4);
        assert(r == m.begin());
        r = m.lower_bound(5);
        assert(r == m.begin());
        r = m.lower_bound(6);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(7);
        assert(r == next(m.begin(), 3));
        r = m.lower_bound(8);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(9);
        assert(r == next(m.begin(), 6));
        r = m.lower_bound(10);
        assert(r == m.end());
    }
    }
#endif
#if _LIBCPP_STD_VER > 11
    {
    typedef std::pair<const int, double> V;
    typedef std::multimap<int, double, std::less<>> M;
    typedef M::iterator R;
    V ar[] =
    {
        V(5, 1),
        V(5, 2),
        V(5, 3),
        V(7, 1),
        V(7, 2),
        V(7, 3),
        V(9, 1),
        V(9, 2),
        V(9, 3)
    };
    M m(ar, ar+sizeof(ar)/sizeof(ar[0]));
    R r = m.lower_bound(4);
    assert(r == m.begin());
    r = m.lower_bound(5);
    assert(r == m.begin());
    r = m.lower_bound(6);
    assert(r == next(m.begin(), 3));
    r = m.lower_bound(7);
    assert(r == next(m.begin(), 3));
    r = m.lower_bound(8);
    assert(r == next(m.begin(), 6));
    r = m.lower_bound(9);
    assert(r == next(m.begin(), 6));
    r = m.lower_bound(10);
    assert(r == m.end());
    }

    {
    typedef PrivateConstructor PC;
    typedef std::multimap<PC, double, std::less<>> M;
    typedef M::iterator R;

    M m;
    m.insert ( std::make_pair<PC, double> ( PC::make(5), 1 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(5), 2 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(5), 3 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(7), 1 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(7), 2 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(7), 3 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(9), 1 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(9), 2 ));
    m.insert ( std::make_pair<PC, double> ( PC::make(9), 3 ));

    R r = m.lower_bound(4);
    assert(r == m.begin());
    r = m.lower_bound(5);
    assert(r == m.begin());
    r = m.lower_bound(6);
    assert(r == next(m.begin(), 3));
    r = m.lower_bound(7);
    assert(r == next(m.begin(), 3));
    r = m.lower_bound(8);
    assert(r == next(m.begin(), 6));
    r = m.lower_bound(9);
    assert(r == next(m.begin(), 6));
    r = m.lower_bound(10);
    assert(r == m.end());
    }

#endif
}
