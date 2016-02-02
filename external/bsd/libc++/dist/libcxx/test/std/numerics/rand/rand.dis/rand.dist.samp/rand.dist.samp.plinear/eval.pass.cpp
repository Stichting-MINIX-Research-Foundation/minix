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
// class piecewise_linear_distribution

// template<class _URNG> result_type operator()(_URNG& g);

#include <iostream>

#include <random>
#include <vector>
#include <iterator>
#include <numeric>
#include <cassert>

template <class T>
inline
T
sqr(T x)
{
    return x*x;
}

double
f(double x, double a, double m, double b, double c)
{
    return a + m*(sqr(x) - sqr(b))/2 + c*(x-b);
}

int main()
{
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14, 16, 17};
        double p[] = {0, 1, 1, 0};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14, 16, 17};
        double p[] = {0, 0, 1, 0};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14, 16, 17};
        double p[] = {1, 0, 0, 0};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14, 16};
        double p[] = {0, 1, 0};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14};
        double p[] = {1, 1};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
    {
        typedef std::piecewise_linear_distribution<> D;
        typedef D::param_type P;
        typedef std::mt19937_64 G;
        G g;
        double b[] = {10, 14, 16, 17};
        double p[] = {25, 62.5, 12.5, 0};
        const size_t Np = sizeof(p) / sizeof(p[0]) - 1;
        D d(b, b+Np+1, p);
        const int N = 1000000;
        std::vector<D::result_type> u;
        for (int i = 0; i < N; ++i)
        {
            D::result_type v = d(g);
            assert(d.min() <= v && v < d.max());
            u.push_back(v);
        }
        std::sort(u.begin(), u.end());
        int kp = -1;
        double a;
        double m;
        double bk;
        double c;
        std::vector<double> areas(Np);
        double S = 0;
        for (int i = 0; i < areas.size(); ++i)
        {
            areas[i] = (p[i]+p[i+1])*(b[i+1]-b[i])/2;
            S += areas[i];
        }
        for (int i = 0; i < areas.size(); ++i)
            areas[i] /= S;
        for (int i = 0; i < Np+1; ++i)
            p[i] /= S;
        for (int i = 0; i < N; ++i)
        {
            int k = std::lower_bound(b, b+Np+1, u[i]) - b - 1;
            if (k != kp)
            {
                a = 0;
                for (int j = 0; j < k; ++j)
                    a += areas[j];
                m = (p[k+1] - p[k]) / (b[k+1] - b[k]);
                bk = b[k];
                c = (b[k+1]*p[k] - b[k]*p[k+1]) / (b[k+1] - b[k]);
                kp = k;
            }
            assert(std::abs(f(u[i], a, m, bk, c) - double(i)/N) < .001);
        }
    }
}
