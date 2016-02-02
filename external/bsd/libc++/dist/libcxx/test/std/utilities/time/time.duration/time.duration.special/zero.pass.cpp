//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <chrono>

// duration

// static constexpr duration zero();

#include <chrono>
#include <cassert>

#include "../../rep.h"

template <class D>
void test()
{
    {
    typedef typename D::rep Rep;
    Rep zero_rep = std::chrono::duration_values<Rep>::zero();
    assert(D::zero().count() == zero_rep);
    }
#ifndef _LIBCPP_HAS_NO_CONSTEXPR
    {
    typedef typename D::rep Rep;
    constexpr Rep zero_rep = std::chrono::duration_values<Rep>::zero();
    static_assert(D::zero().count() == zero_rep, "");
    }
#endif
}

int main()
{
    test<std::chrono::duration<int> >();
    test<std::chrono::duration<Rep> >();
}
