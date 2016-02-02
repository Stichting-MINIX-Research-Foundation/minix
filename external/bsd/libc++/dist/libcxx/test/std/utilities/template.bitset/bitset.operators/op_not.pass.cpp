//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test bitset<N> operator^(const bitset<N>& lhs, const bitset<N>& rhs);

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
void test_op_not()
{
    std::bitset<N> v1 = make_bitset<N>();
    std::bitset<N> v2 = make_bitset<N>();
    std::bitset<N> v3 = v1;
    assert((v1 ^ v2) == (v3 ^= v2));;
}

int main()
{
    test_op_not<0>();
    test_op_not<1>();
    test_op_not<31>();
    test_op_not<32>();
    test_op_not<33>();
    test_op_not<63>();
    test_op_not<64>();
    test_op_not<65>();
    test_op_not<1000>();
}
