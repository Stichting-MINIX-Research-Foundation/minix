//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <set>

// class multiset

// template <class InputIterator>
//     multiset(InputIterator first, InputIterator last);

#include <set>
#include <cassert>

#include "test_iterators.h"
#include "min_allocator.h"

int main()
{
    {
    typedef int V;
    V ar[] =
    {
        1,
        1,
        1,
        2,
        2,
        2,
        3,
        3,
        3
    };
    std::multiset<V> m(input_iterator<const int*>(ar),
                  input_iterator<const int*>(ar+sizeof(ar)/sizeof(ar[0])));
    assert(m.size() == 9);
    assert(distance(m.begin(), m.end()) == 9);
    assert(*next(m.begin(), 0) == 1);
    assert(*next(m.begin(), 1) == 1);
    assert(*next(m.begin(), 2) == 1);
    assert(*next(m.begin(), 3) == 2);
    assert(*next(m.begin(), 4) == 2);
    assert(*next(m.begin(), 5) == 2);
    assert(*next(m.begin(), 6) == 3);
    assert(*next(m.begin(), 7) == 3);
    assert(*next(m.begin(), 8) == 3);
    }
#if __cplusplus >= 201103L
    {
    typedef int V;
    V ar[] =
    {
        1,
        1,
        1,
        2,
        2,
        2,
        3,
        3,
        3
    };
    std::multiset<V, std::less<V>, min_allocator<V>> m(input_iterator<const int*>(ar),
                  input_iterator<const int*>(ar+sizeof(ar)/sizeof(ar[0])));
    assert(m.size() == 9);
    assert(distance(m.begin(), m.end()) == 9);
    assert(*next(m.begin(), 0) == 1);
    assert(*next(m.begin(), 1) == 1);
    assert(*next(m.begin(), 2) == 1);
    assert(*next(m.begin(), 3) == 2);
    assert(*next(m.begin(), 4) == 2);
    assert(*next(m.begin(), 5) == 2);
    assert(*next(m.begin(), 6) == 3);
    assert(*next(m.begin(), 7) == 3);
    assert(*next(m.begin(), 8) == 3);
    }
#endif
}
