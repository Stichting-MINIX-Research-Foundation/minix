//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <random>

// template <class UIntType, size_t w, size_t n, size_t m, size_t r,
//           UIntType a, size_t u, UIntType d, size_t s,
//           UIntType b, size_t t, UIntType c, size_t l, UIntType f>
// class mersenne_twister_engine;

// void discard(unsigned long long z);

#include <random>
#include <sstream>
#include <cassert>

void
test1()
{
    std::mt19937 e1;
    std::mt19937 e2 = e1;
    assert(e1 == e2);
    e1.discard(3);
    assert(e1 != e2);
    e2();
    e2();
    e2();
    assert(e1 == e2);
}

void
test2()
{
    std::mt19937_64 e1;
    std::mt19937_64 e2 = e1;
    assert(e1 == e2);
    e1.discard(3);
    assert(e1 != e2);
    e2();
    e2();
    e2();
    assert(e1 == e2);
}

int main()
{
    test1();
    test2();
}
