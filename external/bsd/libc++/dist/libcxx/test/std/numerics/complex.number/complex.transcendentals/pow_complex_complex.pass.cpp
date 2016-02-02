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
//   complex<T>
//   pow(const complex<T>& x, const complex<T>& y);

#include <complex>
#include <cassert>

#include "../cases.h"

template <class T>
void
test(const std::complex<T>& a, const std::complex<T>& b, std::complex<T> x)
{
    std::complex<T> c = pow(a, b);
    is_about(real(c), real(x));
    is_about(imag(c), imag(x));
}

template <class T>
void
test()
{
    test(std::complex<T>(2, 3), std::complex<T>(2, 0), std::complex<T>(-5, 12));
}

void test_edges()
{
    const double pi = std::atan2(+0., -0.);
    const unsigned N = sizeof(x) / sizeof(x[0]);
    for (unsigned i = 0; i < N; ++i)
    {
        for (unsigned j = 0; j < N; ++j)
        {
            std::complex<double> r = pow(x[i], x[j]);
            std::complex<double> z = exp(x[j] * log(x[i]));
            if (std::isnan(real(r)))
                assert(std::isnan(real(z)));
            else
            {
                assert(real(r) == real(z));
                assert(std::signbit(real(r)) == std::signbit(real(z)));
            }
            if (std::isnan(imag(r)))
                assert(std::isnan(imag(z)));
            else
            {
                assert(imag(r) == imag(z));
                assert(std::signbit(imag(r)) == std::signbit(imag(z)));
            }
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
