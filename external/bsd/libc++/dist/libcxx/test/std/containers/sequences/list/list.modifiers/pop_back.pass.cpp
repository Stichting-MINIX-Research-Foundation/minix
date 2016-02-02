//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// void pop_back();

#if _LIBCPP_DEBUG >= 1
#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))
#endif

#include <list>
#include <cassert>

#include "min_allocator.h"

int main()
{
    {
    int a[] = {1, 2, 3};
    std::list<int> c(a, a+3);
    c.pop_back();
    assert(c == std::list<int>(a, a+2));
    c.pop_back();
    assert(c == std::list<int>(a, a+1));
    c.pop_back();
    assert(c.empty());
#if _LIBCPP_DEBUG >= 1
        c.pop_back();
        assert(false);
#endif        
    }
#if __cplusplus >= 201103L
    {
    int a[] = {1, 2, 3};
    std::list<int, min_allocator<int>> c(a, a+3);
    c.pop_back();
    assert((c == std::list<int, min_allocator<int>>(a, a+2)));
    c.pop_back();
    assert((c == std::list<int, min_allocator<int>>(a, a+1)));
    c.pop_back();
    assert(c.empty());
#if _LIBCPP_DEBUG >= 1
        c.pop_back();
        assert(false);
#endif        
    }
#endif
}
