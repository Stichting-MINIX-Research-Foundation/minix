//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test bitset<N>& operator&=(const bitset<N>& rhs);

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
void test_op_and_eq()
{
    std::bitset<N> v1 = make_bitset<N>();
    std::bitset<N> v2 = make_bitset<N>();
    std::bitset<N> v3 = v1;
    v1 &= v2;
    for (std::size_t i = 0; i < N; ++i)
        assert(v1[i] == (v3[i] && v2[i]));
}

int main()
{
    test_op_and_eq<0>();
    test_op_and_eq<1>();
    test_op_and_eq<31>();
    test_op_and_eq<32>();
    test_op_and_eq<33>();
    test_op_and_eq<63>();
    test_op_and_eq<64>();
    test_op_and_eq<65>();
    test_op_and_eq<1000>();
}
