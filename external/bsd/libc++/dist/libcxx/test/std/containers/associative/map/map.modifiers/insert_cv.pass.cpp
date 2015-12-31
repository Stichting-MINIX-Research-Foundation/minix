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

// pair<iterator, bool> insert(const value_type& v);

#include <map>
#include <cassert>

#include "min_allocator.h"

int main()
{
    {
        typedef std::map<int, double> M;
        typedef std::pair<M::iterator, bool> R;
        M m;
        R r = m.insert(M::value_type(2, 2.5));
        assert(r.second);
        assert(r.first == m.begin());
        assert(m.size() == 1);
        assert(r.first->first == 2);
        assert(r.first->second == 2.5);

        r = m.insert(M::value_type(1, 1.5));
        assert(r.second);
        assert(r.first == m.begin());
        assert(m.size() == 2);
        assert(r.first->first == 1);
        assert(r.first->second == 1.5);

        r = m.insert(M::value_type(3, 3.5));
        assert(r.second);
        assert(r.first == prev(m.end()));
        assert(m.size() == 3);
        assert(r.first->first == 3);
        assert(r.first->second == 3.5);

        r = m.insert(M::value_type(3, 3.5));
        assert(!r.second);
        assert(r.first == prev(m.end()));
        assert(m.size() == 3);
        assert(r.first->first == 3);
        assert(r.first->second == 3.5);
    }
#if __cplusplus >= 201103L
    {
        typedef std::map<int, double, std::less<int>, min_allocator<std::pair<const int, double>>> M;
        typedef std::pair<M::iterator, bool> R;
        M m;
        R r = m.insert(M::value_type(2, 2.5));
        assert(r.second);
        assert(r.first == m.begin());
        assert(m.size() == 1);
        assert(r.first->first == 2);
        assert(r.first->second == 2.5);

        r = m.insert(M::value_type(1, 1.5));
        assert(r.second);
        assert(r.first == m.begin());
        assert(m.size() == 2);
        assert(r.first->first == 1);
        assert(r.first->second == 1.5);

        r = m.insert(M::value_type(3, 3.5));
        assert(r.second);
        assert(r.first == prev(m.end()));
        assert(m.size() == 3);
        assert(r.first->first == 3);
        assert(r.first->second == 3.5);

        r = m.insert(M::value_type(3, 3.5));
        assert(!r.second);
        assert(r.first == prev(m.end()));
        assert(m.size() == 3);
        assert(r.first->first == 3);
        assert(r.first->second == 3.5);
    }
#endif
}
