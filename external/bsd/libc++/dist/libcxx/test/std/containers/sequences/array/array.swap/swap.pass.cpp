//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <array>

// void swap(array& a);

#include <array>
#include <cassert>

#include "../suppress_array_warnings.h"

int main()
{
    {
        typedef double T;
        typedef std::array<T, 3> C;
        C c1 = {1, 2, 3.5};
        C c2 = {4, 5, 6.5};
        c1.swap(c2);
        assert(c1.size() == 3);
        assert(c1[0] == 4);
        assert(c1[1] == 5);
        assert(c1[2] == 6.5);
        assert(c2.size() == 3);
        assert(c2[0] == 1);
        assert(c2[1] == 2);
        assert(c2[2] == 3.5);
    }
    {
        typedef double T;
        typedef std::array<T, 0> C;
        C c1 = {};
        C c2 = {};
        c1.swap(c2);
        assert(c1.size() == 0);
        assert(c2.size() == 0);
    }
}
