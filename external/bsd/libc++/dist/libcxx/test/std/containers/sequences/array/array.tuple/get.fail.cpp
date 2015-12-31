//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <array>

// template <size_t I, class T, size_t N> T& get(array<T, N>& a);

// Prevent -Warray-bounds from issuing a diagnostic when testing with clang verify.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Warray-bounds"
#endif

#include <array>
#include <cassert>

#include "test_macros.h"
#include "../suppress_array_warnings.h"

int main()
{
    {
        typedef double T;
        typedef std::array<T, 3> C;
        C c = {1, 2, 3.5};
        std::get<3>(c) = 5.5; // expected-note {{requested here}}
#if TEST_STD_VER >= 11
        // expected-error@array:* {{static_assert failed "Index out of bounds in std::get<> (std::array)"}}
#else
        // expected-error@array:* {{implicit instantiation of undefined template '__static_assert_test<false>'}}
#endif
    }
}
