//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test bitset<N>& operator<<=(size_t pos);

#include <bitset>
#include <cstdlib>
#include <cassert>

#pragma clang diagnostic ignored "-Wtautological-compare"

template <std::size_t N>
std::bitset<N>
make_bitset()
{
    std::bitset<N> v;
    for (std::size_t i = 0; i < N; ++i)
        v[i] = static_cast<bool>(std::rand() & 1);
    return v;
}

template <std::size_t N>
void test_left_shift()
{
    for (std::size_t s = 0; s <= N+1; ++s)
    {
        std::bitset<N> v1 = make_bitset<N>();
        std::bitset<N> v2 = v1;
        v1 <<= s;
        for (std::size_t i = 0; i < N; ++i)
            if (i < s)
                assert(v1[i] == 0);
            else
                assert(v1[i] == v2[i-s]);
    }
}

int main()
{
    test_left_shift<0>();
    test_left_shift<1>();
    test_left_shift<31>();
    test_left_shift<32>();
    test_left_shift<33>();
    test_left_shift<63>();
    test_left_shift<64>();
    test_left_shift<65>();
    test_left_shift<1000>();
}
