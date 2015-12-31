//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <complex>

// template<class T>
//   T
//   norm(const complex<T>& x);

#include <complex>
#include <cassert>

#include "../cases.h"

template <class T>
void
test()
{
    std::complex<T> z(3, 4);
    assert(norm(z) == 25);
}

void test_edges()
{
    const unsigned N = sizeof(x) / sizeof(x[0]);
    for (unsigned i = 0; i < N; ++i)
    {
        double r = norm(x[i]);
        switch (classify(x[i]))
        {
        case zero:
            assert(r == 0);
            assert(!std::signbit(r));
            break;
        case non_zero:
            assert(std::isfinite(r) && r > 0);
            break;
        case inf:
            assert(std::isinf(r) && r > 0);
            break;
        case NaN:
            assert(std::isnan(r));
            break;
        case non_zero_nan:
            assert(std::isnan(r));
            break;
        }
    }
}

int main()
{
    test<float>();
    test<double>();
    test<long double>();
    test_edges();
}
