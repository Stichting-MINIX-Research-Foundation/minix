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

// template <class InputIterator>
//     void insert(InputIterator first, InputIterator last);

#include <unordered_map>
#include <string>
#include <cassert>

#include "test_iterators.h"
#include "min_allocator.h"

int main()
{
    {
        typedef std::unordered_multimap<int, std::string> C;
        typedef std::pair<int, std::string> P;
        P a[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        C c;
        c.insert(input_iterator<P*>(a), input_iterator<P*>(a + sizeof(a)/sizeof(a[0])));
        assert(c.size() == 6);
        typedef std::pair<C::iterator, C::iterator> Eq;
        Eq eq = c.equal_range(1);
        assert(std::distance(eq.first, eq.second) == 2);
        C::iterator k = eq.first;
        assert(k->first == 1);
        assert(k->second == "one");
        ++k;
        assert(k->first == 1);
        assert(k->second == "four");
        eq = c.equal_range(2);
        assert(std::distance(eq.first, eq.second) == 2);
        k = eq.first;
        assert(k->first == 2);
        assert(k->second == "two");
        ++k;
        assert(k->first == 2);
        assert(k->second == "four");
        eq = c.equal_range(3);
        assert(std::distance(eq.first, eq.second) == 1);
        k = eq.first;
        assert(k->first == 3);
        assert(k->second == "three");
        eq = c.equal_range(4);
        assert(std::distance(eq.first, eq.second) == 1);
        k = eq.first;
        assert(k->first == 4);
        assert(k->second == "four");
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_multimap<int, std::string, std::hash<int>, std::equal_to<int>,
                            min_allocator<std::pair<const int, std::string>>> C;
        typedef std::pair<int, std::string> P;
        P a[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        C c;
        c.insert(input_iterator<P*>(a), input_iterator<P*>(a + sizeof(a)/sizeof(a[0])));
        assert(c.size() == 6);
        typedef std::pair<C::iterator, C::iterator> Eq;
        Eq eq = c.equal_range(1);
        assert(std::distance(eq.first, eq.second) == 2);
        C::iterator k = eq.first;
        assert(k->first == 1);
        assert(k->second == "one");
        ++k;
        assert(k->first == 1);
        assert(k->second == "four");
        eq = c.equal_range(2);
        assert(std::distance(eq.first, eq.second) == 2);
        k = eq.first;
        assert(k->first == 2);
        assert(k->second == "two");
        ++k;
        assert(k->first == 2);
        assert(k->second == "four");
        eq = c.equal_range(3);
        assert(std::distance(eq.first, eq.second) == 1);
        k = eq.first;
        assert(k->first == 3);
        assert(k->second == "three");
        eq = c.equal_range(4);
        assert(std::distance(eq.first, eq.second) == 1);
        k = eq.first;
        assert(k->first == 4);
        assert(k->second == "four");
        assert(std::distance(c.begin(), c.end()) == c.size());
        assert(std::distance(c.cbegin(), c.cend()) == c.size());
    }
#endif
}
