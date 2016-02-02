//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// Call back() on empty const container.

#if _LIBCPP_DEBUG >= 1

#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))

#include <list>
#include <cassert>
#include <iterator>
#include <exception>
#include <cstdlib>

#include "min_allocator.h"

int main()
{
    {
    typedef int T;
    typedef std::list<T> C;
    const C c;
    assert(c.back() == 0);
    assert(false);
    }
#if __cplusplus >= 201103L
    {
    typedef int T;
    typedef std::list<T, min_allocator<T>> C;
    const C c;
    assert(c.back() == 0);
    assert(false);
    }
#endif
}

#else

int main()
{
}

#endif
