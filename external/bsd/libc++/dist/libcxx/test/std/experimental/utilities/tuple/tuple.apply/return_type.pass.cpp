//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11

// <experimental/tuple>

// template <class F, class T> constexpr decltype(auto) apply(F &&, T &&)

// Test the return type deduction.

#include <experimental/tuple>
#include <cassert>

static int my_int = 42;

template <int N> struct index {};

void f(index<0>) {}

int f(index<1>) { return 0; }
int const f(index<2>) { return 0; }
int volatile f(index<3>) { return 0; }
int const volatile f(index<4>) { return 0; }

int & f(index<5>) { return static_cast<int &>(my_int); }
int const & f(index<6>) { return static_cast<int const &>(my_int); }
int volatile & f(index<7>) { return static_cast<int volatile &>(my_int); }
int const volatile & f(index<8>) { return static_cast<int const volatile &>(my_int); }

int && f(index<9>) { return static_cast<int &&>(my_int); }
int const && f(index<10>) { return static_cast<int const &&>(my_int); }
int volatile && f(index<11>) { return static_cast<int volatile &&>(my_int); }
int const volatile && f(index<12>) { return static_cast<int const volatile &&>(my_int); }

int * f(index<13>) { return static_cast<int *>(&my_int); }
int const * f(index<14>) { return static_cast<int const *>(&my_int); }
int volatile * f(index<15>) { return static_cast<int volatile *>(&my_int); }
int const volatile * f(index<16>) { return static_cast<int const volatile *>(&my_int); }


template <int Func, class Expect>
void test()
{
    using F = decltype((f(index<Func>{})));
    static_assert(std::is_same<F, Expect>::value, "");
}

namespace ex = std::experimental;

int main()
{
    test<0, void>();
    test<1, int>();
    //test<2, int const>();
    //test<3, int volatile>();
    //test<4, int const volatile>();
    test<5, int &>();
    test<6, int const &>();
    test<7, int volatile &>();
    test<8, int const volatile &>();
    test<9, int &&>();
    test<10, int const &&>();
    test<11, int volatile &&>();
    test<12, int const volatile &&>();
    test<13, int *>();
    test<14, int const *>();
    test<15, int volatile *>();
    test<16, int const volatile *>();
}
