//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// REQUIRES: long_tests

// <random>

// template<class RealType = double>
// class exponential_distribution

// template<class _URNG> result_type operator()(_URNG& g, const param_type& parm);

#include <random>
#include <cassert>
#include <vector>
#include <numeric>

template <class T>
inline
T
sqr(T x)
{
    return x * x;
}

int main()
{
    {
        typedef std::exponential_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937 G;
        G g;
        D d(.75);
        P p(2);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g, p);
            assert(d.min() < v);
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(), 0.0) / u.size();
        double var = 0;
        double skew = 0;
        double kurtosis = 0;
        for (int i = 0; i < u.size(); ++i)
        {
            double d = (u[i] - mean);
            double d2 = sqr(d);
            var += d2;
            skew += d * d2;
            kurtosis += d2 * d2;
        }
        var /= u.size();
        double dev = std::sqrt(var);
        skew /= u.size() * dev * var;
        kurtosis /= u.size() * var * var;
        kurtosis -= 3;
        double x_mean = 1/p.lambda();
        double x_var = 1/sqr(p.lambda());
        double x_skew = 2;
        double x_kurtosis = 6;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs((skew - x_skew) / x_skew) < 0.01);
        assert(std::abs((kurtosis - x_kurtosis) / x_kurtosis) < 0.01);
    }
}
