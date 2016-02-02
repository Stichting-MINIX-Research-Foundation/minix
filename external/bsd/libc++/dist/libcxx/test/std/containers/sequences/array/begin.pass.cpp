//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <array>

// iterator begin();

#include <array>
#include <cassert>

#include "suppress_array_warnings.h"

int main()
{
    {
        typedef double T;
        typedef std::array<T, 3> C;
        C c = {1, 2, 3.5};
        C::iterator i;
        i = c.begin();
        assert(*i == 1);
        assert(&*i == c.data());
        *i = 5.5;
        assert(c[0] == 5.5);
    }
}
