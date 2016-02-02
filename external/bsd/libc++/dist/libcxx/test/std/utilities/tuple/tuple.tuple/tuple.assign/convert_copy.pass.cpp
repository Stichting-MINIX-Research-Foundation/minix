//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <tuple>

// template <class... Types> class tuple;

// template <class... UTypes>
//   tuple& operator=(const tuple<UTypes...>& u);

// UNSUPPORTED: c++98, c++03

#include <tuple>
#include <string>
#include <cassert>

struct B
{
    int id_;

    explicit B(int i = 0) : id_(i) {}
};

struct D
    : B
{
    explicit D(int i = 0) : B(i) {}
};

int main()
{
    {
        typedef std::tuple<double> T0;
        typedef std::tuple<int> T1;
        T0 t0(2.5);
        T1 t1;
        t1 = t0;
        assert(std::get<0>(t1) == 2);
    }
    {
        typedef std::tuple<double, char> T0;
        typedef std::tuple<int, int> T1;
        T0 t0(2.5, 'a');
        T1 t1;
        t1 = t0;
        assert(std::get<0>(t1) == 2);
        assert(std::get<1>(t1) == int('a'));
    }
    {
        typedef std::tuple<double, char, D> T0;
        typedef std::tuple<int, int, B> T1;
        T0 t0(2.5, 'a', D(3));
        T1 t1;
        t1 = t0;
        assert(std::get<0>(t1) == 2);
        assert(std::get<1>(t1) == int('a'));
        assert(std::get<2>(t1).id_ == 3);
    }
    {
        D d(3);
        D d2(2);
        typedef std::tuple<double, char, D&> T0;
        typedef std::tuple<int, int, B&> T1;
        T0 t0(2.5, 'a', d2);
        T1 t1(1.5, 'b', d);
        t1 = t0;
        assert(std::get<0>(t1) == 2);
        assert(std::get<1>(t1) == int('a'));
        assert(std::get<2>(t1).id_ == 2);
    }
}
