//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <deque>

// void push_back(value_type&& v);
// void pop_back();
// void pop_front();

#include <deque>
#include <cassert>

#include "../../../MoveOnly.h"
#include "min_allocator.h"

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES

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
    C c(init);
    for (int i = 0; i < init-start; ++i)
        c.pop_back();
    for (int i = 0; i < size; ++i)
        c.push_back(MoveOnly(i));
    for (int i = 0; i < start; ++i)
        c.pop_front();
    return c;
};

template <class C>
void test(int size)
{
    int rng[] = {0, 1, 2, 3, 1023, 1024, 1025, 2046, 2047, 2048, 2049};
    const int N = sizeof(rng)/sizeof(rng[0]);
    for (int j = 0; j < N; ++j)
    {
        C c = make<C>(size, rng[j]);
        typename C::const_iterator it = c.begin();
        for (int i = 0; i < size; ++i, ++it)
            assert(*it == MoveOnly(i));
    }
}

#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES

int main()
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
    int rng[] = {0, 1, 2, 3, 1023, 1024, 1025, 2046, 2047, 2048, 2049, 4094, 4095, 4096};
    const int N = sizeof(rng)/sizeof(rng[0]);
    for (int j = 0; j < N; ++j)
        test<std::deque<MoveOnly> >(rng[j]);
    }
#if __cplusplus >= 201103L
    {
    int rng[] = {0, 1, 2, 3, 1023, 1024, 1025, 2046, 2047, 2048, 2049, 4094, 4095, 4096};
    const int N = sizeof(rng)/sizeof(rng[0]);
    for (int j = 0; j < N; ++j)
        test<std::deque<MoveOnly, min_allocator<MoveOnly>> >(rng[j]);
    }
#endif
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
