//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <algorithm>

// template<ForwardIterator Iter, class T, CopyConstructible Compare>
//   requires Predicate<Compare, T, Iter::value_type>
//         && Predicate<Compare, Iter::value_type, T>
//   bool
//   binary_search(Iter first, Iter last, const T& value, Compare comp);

#include <algorithm>
#include <vector>
#include <functional>
#include <cassert>

#include "test_iterators.h"

template <class Iter, class T>
void
test(Iter first, Iter last, const T& value, bool x)
{
    assert(std::binary_search(first, last, value, std::greater<int>()) == x);
}

template <class Iter>
void
test()
{
    const unsigned N = 1000;
    const unsigned M = 10;
    std::vector<int> v(N);
    int x = 0;
    for (int i = 0; i < v.size(); ++i)
    {
        v[i] = x;
        if (++x == M)
            x = 0;
    }
    std::sort(v.begin(), v.end(), std::greater<int>());
    for (x = 0; x < M; ++x)
        test(Iter(v.data()), Iter(v.data()+v.size()), x, true);
    test(Iter(v.data()), Iter(v.data()+v.size()), -1, false);
    test(Iter(v.data()), Iter(v.data()+v.size()), M, false);
}

int main()
{
    int d[] = {6, 4, 2, 0};
    for (int* e = d; e <= d+4; ++e)
        for (int x = -1; x <= 7; ++x)
            test(d, e, x, (x % 2 == 0) && e != d && (-2*(e-d) + 8 <= x));

    test<forward_iterator<const int*> >();
    test<bidirectional_iterator<const int*> >();
    test<random_access_iterator<const int*> >();
    test<const int*>();
}
