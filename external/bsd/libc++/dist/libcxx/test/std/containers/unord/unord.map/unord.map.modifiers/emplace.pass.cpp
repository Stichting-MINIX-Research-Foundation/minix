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

// template <class... Args>
//     pair<iterator, bool> emplace(Args&&... args);

#include <unordered_map>
#include <cassert>

#include "../../../Emplaceable.h"
#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        typedef std::unordered_map<int, Emplaceable> C;
        typedef std::pair<C::iterator, bool> R;
        C c;
        R r = c.emplace(std::piecewise_construct, std::forward_as_tuple(3),
                                                  std::forward_as_tuple());
        assert(r.second);
        assert(c.size() == 1);
        assert(r.first->first == 3);
        assert(r.first->second == Emplaceable());

        r = c.emplace(std::pair<const int, Emplaceable>(4, Emplaceable(5, 6)));
        assert(r.second);
        assert(c.size() == 2);
        assert(r.first->first == 4);
        assert(r.first->second == Emplaceable(5, 6));

        r = c.emplace(std::piecewise_construct, std::forward_as_tuple(5),
                                               std::forward_as_tuple(6, 7));
        assert(r.second);
        assert(c.size() == 3);
        assert(r.first->first == 5);
        assert(r.first->second == Emplaceable(6, 7));
    }
#if __cplusplus >= 201103L
    {
        typedef std::unordered_map<int, Emplaceable, std::hash<int>, std::equal_to<int>,
                            min_allocator<std::pair<const int, Emplaceable>>> C;
        typedef std::pair<C::iterator, bool> R;
        C c;
        R r = c.emplace(std::piecewise_construct, std::forward_as_tuple(3),
                                                  std::forward_as_tuple());
        assert(r.second);
        assert(c.size() == 1);
        assert(r.first->first == 3);
        assert(r.first->second == Emplaceable());

        r = c.emplace(std::pair<const int, Emplaceable>(4, Emplaceable(5, 6)));
        assert(r.second);
        assert(c.size() == 2);
        assert(r.first->first == 4);
        assert(r.first->second == Emplaceable(5, 6));

        r = c.emplace(std::piecewise_construct, std::forward_as_tuple(5),
                                               std::forward_as_tuple(6, 7));
        assert(r.second);
        assert(c.size() == 3);
        assert(r.first->first == 5);
        assert(r.first->second == Emplaceable(6, 7));
    }
#endif
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
