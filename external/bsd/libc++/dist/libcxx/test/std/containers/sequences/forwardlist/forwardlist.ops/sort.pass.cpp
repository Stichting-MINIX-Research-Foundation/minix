//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <forward_list>

// void sort();

#include <forward_list>
#include <iterator>
#include <algorithm>
#include <vector>
#include <cassert>

#include "min_allocator.h"

template <class C>
void test(int N)
{
    typedef typename C::value_type T;
    typedef std::vector<T> V;
    V v;
    for (int i = 0; i < N; ++i)
        v.push_back(i);
    std::random_shuffle(v.begin(), v.end());
    C c(v.begin(), v.end());
    c.sort();
    assert(distance(c.begin(), c.end()) == N);
    typename C::const_iterator j = c.begin();
    for (int i = 0; i < N; ++i, ++j)
        assert(*j == i);
}

int main()
{
    for (int i = 0; i < 40; ++i)
        test<std::forward_list<int> >(i);
#if __cplusplus >= 201103L
    for (int i = 0; i < 40; ++i)
        test<std::forward_list<int, min_allocator<int>> >(i);
#endif
}
