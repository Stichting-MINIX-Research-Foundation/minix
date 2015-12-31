//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <algorithm>

// template<ForwardIterator Iter>
//   requires LessThanComparable<Iter::value_type>
//   Iter
//   max_element(Iter first, Iter last);

#include <algorithm>
#include <cassert>

#include "test_iterators.h"

template <class Iter>
void
test(Iter first, Iter last)
{
    Iter i = std::max_element(first, last);
    if (first != last)
    {
        for (Iter j = first; j != last; ++j)
            assert(!(*i < *j));
    }
    else
        assert(i == last);
}

template <class Iter>
void
test(unsigned N)
{
    int* a = new int[N];
    for (int i = 0; i < N; ++i)
        a[i] = i;
    std::random_shuffle(a, a+N);
    test(Iter(a), Iter(a+N));
    delete [] a;
}

template <class Iter>
void
test()
{
    test<Iter>(0);
    test<Iter>(1);
    test<Iter>(2);
    test<Iter>(3);
    test<Iter>(10);
    test<Iter>(1000);
}

#if __cplusplus >= 201402L
constexpr int il[] = { 2, 4, 6, 8, 7, 5, 3, 1 };
#endif

void constexpr_test()
{
#if __cplusplus >= 201402L
    constexpr auto p = std::max_element(il,il+8);
    static_assert ( *p == 8, "" );
#endif
}

int main()
{
    test<forward_iterator<const int*> >();
    test<bidirectional_iterator<const int*> >();
    test<random_access_iterator<const int*> >();
    test<const int*>();
    
    constexpr_test ();
}
