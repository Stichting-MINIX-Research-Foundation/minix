//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <deque>

// template <class InputIterator>
//   void assign(InputIterator f, InputIterator l);

#include <deque>
#include <cassert>

#include "test_iterators.h"
#include "min_allocator.h"

template <class C>
C
make(int size, int start = 0 )
{
    const int b = 4096 / sizeof(int);
    int init = 0;
    if (start > 0)
    {
        init = (start+1) / b + ((start+1) % b != 0);
        init *= b;
        --init;
    }
    C c(init, 0);
    for (int i = 0; i < init-start; ++i)
        c.pop_back();
    for (int i = 0; i < size; ++i)
        c.push_back(i);
    for (int i = 0; i < start; ++i)
        c.pop_front();
    return c;
};

template <class C>
void
test(C& c1, const C& c2)
{
    std::size_t c1_osize = c1.size();
    c1.assign(c2.begin(), c2.end());
    assert(distance(c1.begin(), c1.end()) == c1.size());
    assert(c1 == c2);
}

template <class C>
void
testN(int start, int N, int M)
{
    typedef typename C::iterator I;
    typedef typename C::const_iterator CI;
    C c1 = make<C>(N, start);
    C c2 = make<C>(M);
    test(c1, c2);
}

template <class C>
void
testI(C& c1, const C& c2)
{
    typedef typename C::const_iterator CI;
    typedef input_iterator<CI> ICI;
    std::size_t c1_osize = c1.size();
    c1.assign(ICI(c2.begin()), ICI(c2.end()));
    assert(distance(c1.begin(), c1.end()) == c1.size());
    assert(c1 == c2);
}

template <class C>
void
testNI(int start, int N, int M)
{
    typedef typename C::iterator I;
    typedef typename C::const_iterator CI;
    C c1 = make<C>(N, start);
    C c2 = make<C>(M);
    testI(c1, c2);
}

int main()
{
    {
    int rng[] = {0, 1, 2, 3, 1023, 1024, 1025, 2047, 2048, 2049};
    const int N = sizeof(rng)/sizeof(rng[0]);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            for (int k = 0; k < N; ++k)
                testN<std::deque<int> >(rng[i], rng[j], rng[k]);
    testNI<std::deque<int> >(1500, 2000, 1000);
    }
#if __cplusplus >= 201103L
    {
    int rng[] = {0, 1, 2, 3, 1023, 1024, 1025, 2047, 2048, 2049};
    const int N = sizeof(rng)/sizeof(rng[0]);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            for (int k = 0; k < N; ++k)
                testN<std::deque<int, min_allocator<int>> >(rng[i], rng[j], rng[k]);
    testNI<std::deque<int, min_allocator<int>> >(1500, 2000, 1000);
    }
#endif
}
