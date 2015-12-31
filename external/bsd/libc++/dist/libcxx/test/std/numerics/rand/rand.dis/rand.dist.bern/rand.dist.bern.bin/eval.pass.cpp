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

// template<class IntType = int>
// class binomial_distribution

// template<class _URNG> result_type operator()(_URNG& g);

#include <random>
#include <numeric>
#include <vector>
#include <cassert>

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
        typedef std::binomial_distribution<> D;
        typedef std::mt19937_64 G;
        G g;
        D d(5, .75);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs((skew - x_skew) / x_skew) < 0.01);
        assert(std::abs((kurtosis - x_kurtosis) / x_kurtosis) < 0.04);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(30, .03125);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs((skew - x_skew) / x_skew) < 0.01);
        assert(std::abs((kurtosis - x_kurtosis) / x_kurtosis) < 0.01);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(40, .25);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs((skew - x_skew) / x_skew) < 0.03);
        assert(std::abs((kurtosis - x_kurtosis) / x_kurtosis) < 0.3);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(40, 0);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        // In this case:
        //   skew     computes to 0./0. == nan
        //   kurtosis computes to 0./0. == nan
        //   x_skew     == inf
        //   x_kurtosis == inf
        //   These tests are commented out because UBSan warns about division by 0
//        skew /= u.size() * dev * var;
//        kurtosis /= u.size() * var * var;
//        kurtosis -= 3;
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
//        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
//        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(mean == x_mean);
        assert(var == x_var);
//        assert(skew == x_skew);
//        assert(kurtosis == x_kurtosis);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(40, 1);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        // In this case:
        //   skew     computes to 0./0. == nan
        //   kurtosis computes to 0./0. == nan
        //   x_skew     == -inf
        //   x_kurtosis == inf
        //   These tests are commented out because UBSan warns about division by 0
//        skew /= u.size() * dev * var;
//        kurtosis /= u.size() * var * var;
//        kurtosis -= 3;
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
//        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
//        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(mean == x_mean);
        assert(var == x_var);
//        assert(skew == x_skew);
//        assert(kurtosis == x_kurtosis);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(400, 0.5);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs(skew - x_skew) < 0.01);
        assert(std::abs(kurtosis - x_kurtosis) < 0.01);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(1, 0.5);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(std::abs((mean - x_mean) / x_mean) < 0.01);
        assert(std::abs((var - x_var) / x_var) < 0.01);
        assert(std::abs(skew - x_skew) < 0.01);
        assert(std::abs((kurtosis - x_kurtosis) / x_kurtosis) < 0.01);
    }
    {
        const int N = 100000;
        std::mt19937 gen1;
        std::mt19937 gen2;

        std::binomial_distribution<>         dist1(5, 0.1);
        std::binomial_distribution<unsigned> dist2(5, 0.1);

        for(int i = 0; i < N; ++i)
            assert(dist1(gen1) == dist2(gen2));
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(0, 0.005);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        // In this case:
        //   skew     computes to 0./0. == nan
        //   kurtosis computes to 0./0. == nan
        //   x_skew     == inf
        //   x_kurtosis == inf
        //   These tests are commented out because UBSan warns about division by 0
//        skew /= u.size() * dev * var;
//        kurtosis /= u.size() * var * var;
//        kurtosis -= 3;
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
//        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
//        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(mean == x_mean);
        assert(var == x_var);
//        assert(skew == x_skew);
//        assert(kurtosis == x_kurtosis);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(0, 0);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        // In this case:
        //   skew     computes to 0./0. == nan
        //   kurtosis computes to 0./0. == nan
        //   x_skew     == inf
        //   x_kurtosis == inf
        //   These tests are commented out because UBSan warns about division by 0
//        skew /= u.size() * dev * var;
//        kurtosis /= u.size() * var * var;
//        kurtosis -= 3;
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
//        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
//        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(mean == x_mean);
        assert(var == x_var);
//        assert(skew == x_skew);
//        assert(kurtosis == x_kurtosis);
    }
    {
        typedef std::binomial_distribution<> D;
        typedef std::mt19937 G;
        G g;
        D d(0, 1);
        const int N = 100000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v <= d.max());
            u.push_back(v);
        }
        double mean = std::accumulate(u.begin(), u.end(),
                                              double(0)) / u.size();
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
        // In this case:
        //   skew     computes to 0./0. == nan
        //   kurtosis computes to 0./0. == nan
        //   x_skew     == -inf
        //   x_kurtosis == inf
        //   These tests are commented out because UBSan warns about division by 0
//        skew /= u.size() * dev * var;
//        kurtosis /= u.size() * var * var;
//        kurtosis -= 3;
        double x_mean = d.t() * d.p();
        double x_var = x_mean*(1-d.p());
//        double x_skew = (1-2*d.p()) / std::sqrt(x_var);
//        double x_kurtosis = (1-6*d.p()*(1-d.p())) / x_var;
        assert(mean == x_mean);
        assert(var == x_var);
//        assert(skew == x_skew);
//        assert(kurtosis == x_kurtosis);
    }
}
