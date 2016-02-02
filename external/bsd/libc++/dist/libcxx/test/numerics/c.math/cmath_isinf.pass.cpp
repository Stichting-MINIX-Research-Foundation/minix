//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <cmath>

// isinf

// XFAIL: linux

#include <cmath>
#include <type_traits>
#include <cassert>

int main()
{
#ifdef isinf
#error isinf defined
#endif
    static_assert((std::is_same<decltype(std::isinf((float)0)), bool>::value), "");
    static_assert((std::is_same<decltype(std::isinf((double)0)), bool>::value), "");
    static_assert((std::is_same<decltype(std::isinf(0)), bool>::value), "");
    static_assert((std::is_same<decltype(std::isinf((long double)0)), bool>::value), "");
    assert(std::isinf(-1.0) == false);
}