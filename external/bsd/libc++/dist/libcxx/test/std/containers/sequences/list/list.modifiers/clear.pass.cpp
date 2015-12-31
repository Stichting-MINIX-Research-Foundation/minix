//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// void clear();

#include <list>
#include <cassert>

#include "min_allocator.h"

int main()
{
    {
    int a[] = {1, 2, 3};
    std::list<int> c(a, a+3);
    c.clear();
    assert(c.empty());
    }
#if __cplusplus >= 201103L
    {
    int a[] = {1, 2, 3};
    std::list<int, min_allocator<int>> c(a, a+3);
    c.clear();
    assert(c.empty());
    }
#endif
}
