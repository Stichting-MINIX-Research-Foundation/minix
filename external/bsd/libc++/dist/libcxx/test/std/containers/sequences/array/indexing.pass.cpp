//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <array>

// reference operator[] (size_type)
// const_reference operator[] (size_type); // constexpr in C++14
// reference at (size_type)
// const_reference at (size_type); // constexpr in C++14

#include <array>
#include <cassert>

#include "test_macros.h"

#include "suppress_array_warnings.h"

int main()
{
    {
        typedef double T;
        typedef std::array<T, 3> C;
        C c = {1, 2, 3.5};
        C::reference r1 = c[0];
        assert(r1 == 1);
        r1 = 5.5;
        assert(c.front() == 5.5);
        
        C::reference r2 = c[2];
        assert(r2 == 3.5);
        r2 = 7.5;
        assert(c.back() == 7.5);
    }
    {
        typedef double T;
        typedef std::array<T, 3> C;
        const C c = {1, 2, 3.5};
        C::const_reference r1 = c[0];
        assert(r1 == 1);
        C::const_reference r2 = c[2];
        assert(r2 == 3.5);
    }

#if TEST_STD_VER > 11
    {
        typedef double T;
        typedef std::array<T, 3> C;
        constexpr C c = {1, 2, 3.5};

        constexpr T t1 = c[0];
        static_assert (t1 == 1, "");

        constexpr T t2 = c[2];
        static_assert (t2 == 3.5, "");
    }
#endif

}
