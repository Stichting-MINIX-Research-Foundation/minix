//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <list>

// explicit list(size_type n);

#include <list>
#include <cassert>
#include "DefaultOnly.h"
#include "../../../stack_allocator.h"
#include "min_allocator.h"

template <class T, class Allocator>
void
test3(unsigned n, Allocator const &alloc = Allocator())
{
#if _LIBCPP_STD_VER > 11
    typedef std::list<T, Allocator> C;
    typedef typename C::const_iterator const_iterator;
    {
    C d(n, alloc);
    assert(d.size() == n);
    assert(std::distance(d.begin(), d.end()) == n);
    assert(d.get_allocator() == alloc);
    }
#endif
}


int main()
{
    {
        std::list<int> l(3);
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
        std::list<int>::const_iterator i = l.begin();
        assert(*i == 0);
        ++i;
        assert(*i == 0);
        ++i;
        assert(*i == 0);
    }
    {
        std::list<int, stack_allocator<int, 3> > l(3);
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
        std::list<int>::const_iterator i = l.begin();
        assert(*i == 0);
        ++i;
        assert(*i == 0);
        ++i;
        assert(*i == 0);
    }
#if _LIBCPP_STD_VER > 11
    {
        typedef std::list<int, min_allocator<int> > C;
        C l(3, min_allocator<int> ());
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
        C::const_iterator i = l.begin();
        assert(*i == 0);
        ++i;
        assert(*i == 0);
        ++i;
        assert(*i == 0);
        test3<int, min_allocator<int>> (3);
    }
#endif
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        std::list<DefaultOnly> l(3);
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
    }
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
#if __cplusplus >= 201103L
    {
        std::list<int, min_allocator<int>> l(3);
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
        std::list<int, min_allocator<int>>::const_iterator i = l.begin();
        assert(*i == 0);
        ++i;
        assert(*i == 0);
        ++i;
        assert(*i == 0);
    }
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        std::list<DefaultOnly, min_allocator<DefaultOnly>> l(3);
        assert(l.size() == 3);
        assert(std::distance(l.begin(), l.end()) == 3);
    }
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
#endif
}
