//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <forward_list>

// explicit forward_list(size_type n);
// explicit forward_list(size_type n, const Alloc& a);

#include <forward_list>
#include <cassert>

#include "DefaultOnly.h"
#include "min_allocator.h"

template <class T, class Allocator>
void check_allocator(unsigned n, Allocator const &alloc = Allocator())
{
#if _LIBCPP_STD_VER > 11
    typedef std::forward_list<T, Allocator> C;
    C d(n, alloc);
    assert(d.get_allocator() == alloc);
    assert(std::distance(d.begin(), d.end()) == n);
#endif
}

int main()
{
    {
        typedef DefaultOnly T;
        typedef std::forward_list<T> C;
        unsigned N = 10;
        C c(N);
        unsigned n = 0;
        for (C::const_iterator i = c.begin(), e = c.end(); i != e; ++i, ++n)
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
            assert(*i == T());
#else
            ;
#endif
        assert(n == N);
    }
#if __cplusplus >= 201103L
    {
        typedef DefaultOnly T;
        typedef std::forward_list<T, min_allocator<T>> C;
        unsigned N = 10;
        C c(N);
        unsigned n = 0;
        for (C::const_iterator i = c.begin(), e = c.end(); i != e; ++i, ++n)
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
            assert(*i == T());
#else
            ;
#endif
        assert(n == N);
        check_allocator<T, min_allocator<T>> ( 0 );
        check_allocator<T, min_allocator<T>> ( 3 );
    }
#endif
}
