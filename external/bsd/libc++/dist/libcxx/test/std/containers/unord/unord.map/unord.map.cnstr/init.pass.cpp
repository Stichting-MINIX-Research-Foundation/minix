//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_map>

// template <class Key, class T, class Hash = hash<Key>, class Pred = equal_to<Key>,
//           class Alloc = allocator<pair<const Key, T>>>
// class unordered_map

// unordered_map(initializer_list<value_type> il);

#include <unordered_map>
#include <string>
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
        typedef std::unordered_map<int, std::string,
                                   test_hash<std::hash<int> >,
                                   test_compare<std::equal_to<int> >,
                                   test_allocator<std::pair<const int, std::string> >
                                   > C;
        typedef std::pair<int, std::string> P;
        C c = {
                P(1, "one"),
                P(2, "two"),
                P(3, "three"),
                P(4, "four"),
                P(1, "four"),
                P(2, "four"),
              };
        assert(c.bucket_count() >= 5);
        assert(c.size() == 4);
        assert(c.at(1) == "one");
        assert(c.at(2) == "two");
        assert(c.at(3) == "three");
        assert(c.at(4) == "four");
        assert(c.hash_function() == test_hash<std::hash<int> >());
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() ==
               (test_allocator<std::pair<const int, std::string> >()));
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_map<int, std::string,
                                   test_hash<std::hash<int> >,
                                   test_compare<std::equal_to<int> >,
                                   min_allocator<std::pair<const int, std::string> >
                                   > C;
        typedef std::pair<int, std::string> P;
        C c = {
                P(1, "one"),
                P(2, "two"),
                P(3, "three"),
                P(4, "four"),
                P(1, "four"),
                P(2, "four"),
              };
        assert(c.bucket_count() >= 5);
        assert(c.size() == 4);
        assert(c.at(1) == "one");
        assert(c.at(2) == "two");
        assert(c.at(3) == "three");
        assert(c.at(4) == "four");
        assert(c.hash_function() == test_hash<std::hash<int> >());
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() ==
               (min_allocator<std::pair<const int, std::string> >()));
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
#if _LIBCPP_STD_VER > 11
    {
        typedef std::pair<int, std::string> P;
        typedef test_allocator<std::pair<const int, std::string>> A;
        typedef test_hash<std::hash<int>> HF;
        typedef test_compare<std::equal_to<int>> Comp;
        typedef std::unordered_map<int, std::string, HF, Comp, A> C;

        A a(42);
        C c ( {
                P(1, "one"),
                P(2, "two"),
                P(3, "three"),
                P(4, "four"),
                P(1, "four"),
                P(2, "four"),
              }, 12, a);
        assert(c.bucket_count() >= 12);
        assert(c.size() == 4);
        assert(c.at(1) == "one");
        assert(c.at(2) == "two");
        assert(c.at(3) == "three");
        assert(c.at(4) == "four");
        assert(c.hash_function() == test_hash<std::hash<int> >());
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() == a);
        assert(!c.empty());
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
        assert(fabs(c.load_factor() - (float)c.size()/c.bucket_count()) < FLT_EPSILON);
        assert(c.max_load_factor() == 1);
    }
    {
        typedef std::pair<int, std::string> P;
        typedef test_allocator<std::pair<const int, std::string>> A;
        typedef test_hash<std::hash<int>> HF;
        typedef test_compare<std::equal_to<int>> Comp;
        typedef std::unordered_map<int, std::string, HF, Comp, A> C;

        HF hf(42);
        A a(43);
        C c ( {
                P(1, "one"),
                P(2, "two"),
                P(3, "three"),
                P(4, "four"),
                P(1, "four"),
                P(2, "four"),
              }, 12, hf, a);
        assert(c.bucket_count() >= 12);
        assert(c.size() == 4);
        assert(c.at(1) == "one");
        assert(c.at(2) == "two");
        assert(c.at(3) == "three");
        assert(c.at(4) == "four");
        assert(c.hash_function() == hf);
        assert(!(c.hash_function() == test_hash<std::hash<int> >()));
        assert(c.key_eq() == test_compare<std::equal_to<int> >());
        assert(c.get_allocator() == a);
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
