//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <valarray>

// template<class T> class valarray;

// template<class T>
//   valarray<T>
//   pow(const valarray<T>& x, const T& y);

#include <valarray>
#include <cassert>
#include <sstream>

bool is_about(double x, double y, int p)
{
    std::ostringstream o;
    o.precision(p);
    scientific(o);
    o << x;
    std::string a = o.str();
    o.str("");
    o << y;
    return a == o.str();
}

int main()
{
    {
        typedef double T;
        T a1[] = {.9, .5, 0., .5, .75};
        T a3[] = {8.1000000000000005e-01,
                  2.5000000000000000e-01,
                  0.0000000000000000e+00,
                  2.5000000000000000e-01,
                  5.6250000000000000e-01};
        const unsigned N = sizeof(a1)/sizeof(a1[0]);
        std::valarray<T> v1(a1, N);
        std::valarray<T> v3 = pow(v1, 2.0);
        assert(v3.size() == v1.size());
        for (int i = 0; i < v3.size(); ++i)
            assert(is_about(v3[i], a3[i], 10));
    }
}
