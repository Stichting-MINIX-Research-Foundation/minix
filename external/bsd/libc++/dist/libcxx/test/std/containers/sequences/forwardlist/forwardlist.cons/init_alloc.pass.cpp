//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <forward_list>

// forward_list(initializer_list<value_type> il, const allocator_type& a);

#include <forward_list>
#include <cassert>

#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    {
        typedef int T;
        typedef test_allocator<T> A;
        typedef std::forward_list<T, A> C;
        C c({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, A(14));
        unsigned n = 0;
        for (C::const_iterator i = c.begin(), e = c.end(); i != e; ++i, ++n)
            assert(*i == n);
        assert(n == 10);
        assert(c.get_allocator() == A(14));
    }
#if __cplusplus >= 201103L
    {
        typedef int T;
        typedef min_allocator<T> A;
        typedef std::forward_list<T, A> C;
        C c({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, A());
        unsigned n = 0;
        for (C::const_iterator i = c.begin(), e = c.end(); i != e; ++i, ++n)
            assert(*i == n);
        assert(n == 10);
        assert(c.get_allocator() == A());
    }
#endif
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
