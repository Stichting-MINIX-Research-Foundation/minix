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

// unordered_multiset(initializer_list<value_type> il);

#include <unordered_set>
#include <cassert>
#include <cfloat>

#include "../../../test_compare.h"
#include "../../../test_hash.h"
#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    {
        typedef std::unordered_multiset<int,
                                   test_hash<std::hash<int> >,
                                   test_compare<std::equal_to<int> >,
                                   test_allocator<int>
                                   > C;
        typedef int P;
        C c = {
                P(1),
                P(2),
                P(3),
                P(4),
                P(1),
                P(2)
            };
        assert(c.bucket_count() >= 7);
        assert(c.size() == 6);
        assert(c.count(1) == 2);
        assert(c.count(2) == 2);
        assert(c.count(3) == 1);
        assert(c.count(4) == 1);
        assert(c.hash_function() == test_hash<std::hash<int> >());
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() == test_allocator<int>());
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_multiset<int,
                                   test_hash<std::hash<int> >,
                                   test_compare<std::equal_to<int> >,
                                   min_allocator<int>
                                   > C;
        typedef int P;
        C c = {
                P(1),
                P(2),
                P(3),
                P(4),
                P(1),
                P(2)
            };
        assert(c.bucket_count() >= 7);
        assert(c.size() == 6);
        assert(c.count(1) == 2);
        assert(c.count(2) == 2);
        assert(c.count(3) == 1);
        assert(c.count(4) == 1);
        assert(c.hash_function() == test_hash<std::hash<int> >());
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() == min_allocator<int>());
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
#if _LIBCPP_STD_VER > 11
    {
        typedef int T;
        typedef test_hash<std::hash<T>> HF;
        typedef test_compare<std::equal_to<T>> Comp;
        typedef test_allocator<T> A;
        typedef std::unordered_multiset<T, HF, Comp, A> C;

        A a(42);
        C c({
                T(1),
                T(2),
                T(3),
                T(4),
                T(1),
                T(2)
            }, 12, a);

        assert(c.bucket_count() >= 12);
        assert(c.size() == 6);
        assert(c.count(1) == 2);
        assert(c.count(2) == 2);
        assert(c.count(3) == 1);
        assert(c.count(4) == 1);
        assert(c.hash_function() == HF());
        assert(c.key_eq() == Comp());
        assert(c.get_allocator() == a);
        assert(!(c.get_allocator() == A()));
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
    {
        typedef int T;
        typedef test_hash<std::hash<T>> HF;
        typedef test_compare<std::equal_to<T>> Comp;
        typedef test_allocator<T> A;
        typedef std::unordered_multiset<T, HF, Comp, A> C;

        A a(42);
        HF hf(43);
        C c({
                T(1),
                T(2),
                T(3),
                T(4),
                T(1),
                T(2)
            }, 12, hf, a);

        assert(c.bucket_count() >= 12);
        assert(c.size() == 6);
        assert(c.count(1) == 2);
        assert(c.count(2) == 2);
        assert(c.count(3) == 1);
        assert(c.count(4) == 1);
        assert(c.hash_function() == hf);
        assert(!(c.hash_function() == HF()));
        assert(c.key_eq() == Comp());
        assert(c.get_allocator() == a);
        assert(!(c.get_allocator() == A()));
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
#endif
#endif
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
