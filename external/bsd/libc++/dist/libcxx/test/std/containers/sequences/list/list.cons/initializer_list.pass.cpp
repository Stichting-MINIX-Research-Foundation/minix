//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// list(initializer_list<value_type> il);

#include <list>
#include <cassert>

#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    {
    std::list<int> d = {3, 4, 5, 6};
    assert(d.size() == 4);
    std::list<int>::iterator i = d.begin();
    assert(*i++ == 3);
    assert(*i++ == 4);
    assert(*i++ == 5);
    assert(*i++ == 6);
    }
#if __cplusplus >= 201103L
    {
    std::list<int, min_allocator<int>> d = {3, 4, 5, 6};
    assert(d.size() == 4);
    std::list<int, min_allocator<int>>::iterator i = d.begin();
    assert(*i++ == 3);
    assert(*i++ == 4);
    assert(*i++ == 5);
    assert(*i++ == 6);
    }
#endif
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
