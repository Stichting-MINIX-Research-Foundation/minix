//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test unsigned long long to_ullong() const;

#include <bitset>
#include <algorithm>
#include <climits>
#include <cassert>

template <std::size_t N>
void test_to_ullong()
{
    const std::size_t M = sizeof(unsigned long long) * CHAR_BIT < N ? sizeof(unsigned long long) * CHAR_BIT : N;
    const std::size_t X = M == 0 ? sizeof(unsigned long long) * CHAR_BIT - 1 : sizeof(unsigned long long) * CHAR_BIT - M;
    const unsigned long long max = M == 0 ? 0 : (unsigned long long)(-1) >> X;
    unsigned long long tests[] = {0,
                           std::min<unsigned long long>(1, max),
                           std::min<unsigned long long>(2, max),
                           std::min<unsigned long long>(3, max),
                           std::min(max, max-3),
                           std::min(max, max-2),
                           std::min(max, max-1),
                           max};
    for (std::size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i)
    {
        unsigned long long j = tests[i];
        std::bitset<N> v(j);
        assert(j == v.to_ullong());
    }
}

int main()
{
    test_to_ullong<0>();
    test_to_ullong<1>();
    test_to_ullong<31>();
    test_to_ullong<32>();
    test_to_ullong<33>();
    test_to_ullong<63>();
    test_to_ullong<64>();
    test_to_ullong<65>();
    test_to_ullong<1000>();
}
