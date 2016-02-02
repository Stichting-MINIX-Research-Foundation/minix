//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_set>

// template <class Value, class Hash = hash<Value>, class Pred = equal_to<Value>,
//           class Alloc = allocator<Value>>
// class unordered_multiset

// size_type bucket_size(size_type n) const

#ifdef _LIBCPP_DEBUG
#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))
#endif

#include <unordered_set>
#include <cassert>

#include "min_allocator.h"

int main()
{
    {
        typedef std::unordered_multiset<int> C;
        typedef int P;
        P a[] =
        {
            P(1),
            P(2),
            P(3),
            P(4),
            P(1),
            P(2)
        };
        const C c(std::begin(a), std::end(a));
        assert(c.bucket_count() >= 7);
        assert(c.bucket_size(0) == 0);
        assert(c.bucket_size(1) == 2);
        assert(c.bucket_size(2) == 2);
        assert(c.bucket_size(3) == 1);
        assert(c.bucket_size(4) == 1);
        assert(c.bucket_size(5) == 0);
        assert(c.bucket_size(6) == 0);
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_multiset<int, std::hash<int>,
                                      std::equal_to<int>, min_allocator<int>> C;
        typedef int P;
        P a[] =
        {
            P(1),
            P(2),
            P(3),
            P(4),
            P(1),
            P(2)
        };
        const C c(std::begin(a), std::end(a));
        assert(c.bucket_count() >= 7);
        assert(c.bucket_size(0) == 0);
        assert(c.bucket_size(1) == 2);
        assert(c.bucket_size(2) == 2);
        assert(c.bucket_size(3) == 1);
        assert(c.bucket_size(4) == 1);
        assert(c.bucket_size(5) == 0);
        assert(c.bucket_size(6) == 0);
    }
#endif
#if _LIBCPP_DEBUG_LEVEL >= 1
    {
        typedef std::unordered_multiset<int> C;
        C c;
        C::size_type i = c.bucket_size(3);
        assert(false);
    }
#endif
}
