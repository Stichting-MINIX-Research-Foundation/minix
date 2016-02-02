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

// void swap(unordered_multimap& __u);

#include <unordered_map>
#include <string>
#include <cassert>

#include "../../test_compare.h"
#include "../../test_hash.h"
#include "test_allocator.h"

#include "min_allocator.h"

int main()
{
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef test_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        C c1(0, Hash(1), Compare(1), Alloc(1));
        C c2(0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(1));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(2));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef test_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(0, Hash(1), Compare(1), Alloc(1));
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(1));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(2));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef test_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc(1));
        C c2(0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(1));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(2));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef test_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc(1));
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(1));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(2));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }

    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef other_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        C c1(0, Hash(1), Compare(1), Alloc(1));
        C c2(0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(2));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(1));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef other_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(0, Hash(1), Compare(1), Alloc(1));
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(2));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(1));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef other_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc(1));
        C c2(0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(2));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(1));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef other_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc(1));
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc(2));
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc(2));
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc(1));
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
#if __cplusplus >= 201103L
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef min_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        C c1(0, Hash(1), Compare(1), Alloc());
        C c2(0, Hash(2), Compare(2), Alloc());
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc());
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc());
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef min_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(0, Hash(1), Compare(1), Alloc());
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc());
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc());
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() == 0);
        assert(c2.size() == 0);
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc());
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef min_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc());
        C c2(0, Hash(2), Compare(2), Alloc());
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() == 0);
        assert(c1.size() == 0);
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc());
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc());
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
    {
        typedef test_hash<std::hash<int> > Hash;
        typedef test_compare<std::equal_to<int> > Compare;
        typedef min_allocator<std::pair<const int, std::string> > Alloc;
        typedef std::unordered_multimap<int, std::string, Hash, Compare, Alloc> C;
        typedef std::pair<int, std::string> P;
        P a1[] =
        {
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four"),
        };
        P a2[] =
        {
            P(10, "ten"),
            P(20, "twenty"),
            P(30, "thirty"),
            P(40, "forty"),
            P(50, "fifty"),
            P(60, "sixty"),
            P(70, "seventy"),
            P(80, "eighty"),
        };
        C c1(std::begin(a1), std::end(a1), 0, Hash(1), Compare(1), Alloc());
        C c2(std::begin(a2), std::end(a2), 0, Hash(2), Compare(2), Alloc());
        c2.max_load_factor(2);
        c1.swap(c2);

        assert(c1.bucket_count() >= 11);
        assert(c1.size() == 8);
        assert(c1.find(10)->second == "ten");
        assert(c1.find(20)->second == "twenty");
        assert(c1.find(30)->second == "thirty");
        assert(c1.find(40)->second == "forty");
        assert(c1.find(50)->second == "fifty");
        assert(c1.find(60)->second == "sixty");
        assert(c1.find(70)->second == "seventy");
        assert(c1.find(80)->second == "eighty");
        assert(c1.hash_function() == Hash(2));
        assert(c1.key_eq() == Compare(2));
        assert(c1.get_allocator() == Alloc());
        assert(std::distance(c1.begin(), c1.end()) == c1.size());
        assert(std::distance(c1.cbegin(), c1.cend()) == c1.size());
        assert(c1.max_load_factor() == 2);

        assert(c2.bucket_count() >= 7);
        assert(c2.size() == 6);
        assert(c2.find(1)->second == "one");
        assert(next(c2.find(1))->second == "four");
        assert(c2.find(2)->second == "two");
        assert(next(c2.find(2))->second == "four");
        assert(c2.find(3)->second == "three");
        assert(c2.find(4)->second == "four");
        assert(c2.hash_function() == Hash(1));
        assert(c2.key_eq() == Compare(1));
        assert(c2.get_allocator() == Alloc());
        assert(std::distance(c2.begin(), c2.end()) == c2.size());
        assert(std::distance(c2.cbegin(), c2.cend()) == c2.size());
        assert(c2.max_load_factor() == 1);
    }
#endif
}
