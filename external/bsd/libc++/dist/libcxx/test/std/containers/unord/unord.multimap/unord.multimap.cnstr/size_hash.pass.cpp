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
// class unordered_multimap

// unordered_multimap(size_type n, const hasher& hf);

#include <unordered_map>
#include <cassert>

#include "../../../NotConstructible.h"
#include "../../../test_compare.h"
#include "../../../test_hash.h"
#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
    {
        typedef std::unordered_multimap<NotConstructible, NotConstructible,
                                   test_hash<std::hash<NotConstructible> >,
                                   test_compare<std::equal_to<NotConstructible> >,
                                   test_allocator<std::pair<const NotConstructible,
                                                                  NotConstructible> >
                                   > C;
        C c(7,
            test_hash<std::hash<NotConstructible> >(8)
           );
        assert(c.bucket_count() == 7);
        assert(c.hash_function() == test_hash<std::hash<NotConstructible> >(8));
        assert(c.key_eq() == test_compare<std::equal_to<NotConstructible> >());
        assert(c.get_allocator() ==
               (test_allocator<std::pair<const NotConstructible, NotConstructible> >()));
        assert(c.size() == 0);
        assert(c.empty());
        assert(std::distance(c.begin(), c.end()) == 0);
        assert(c.load_factor() == 0);
        assert(c.max_load_factor() == 1);
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_multimap<NotConstructible, NotConstructible,
                                   test_hash<std::hash<NotConstructible> >,
                                   test_compare<std::equal_to<NotConstructible> >,
                                   min_allocator<std::pair<const NotConstructible,
                                                                 NotConstructible> >
                                   > C;
        C c(7,
            test_hash<std::hash<NotConstructible> >(8)
           );
        assert(c.bucket_count() == 7);
        assert(c.hash_function() == test_hash<std::hash<NotConstructible> >(8));
        assert(c.key_eq() == test_compare<std::equal_to<NotConstructible> >());
        assert(c.get_allocator() ==
               (min_allocator<std::pair<const NotConstructible, NotConstructible> >()));
        assert(c.size() == 0);
        assert(c.empty());
        assert(std::distance(c.begin(), c.end()) == 0);
        assert(c.load_factor() == 0);
        assert(c.max_load_factor() == 1);
    }
#endif
}
