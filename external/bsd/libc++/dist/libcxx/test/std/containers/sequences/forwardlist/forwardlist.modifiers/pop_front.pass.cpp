//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <forward_list>

// void pop_front();

#include <forward_list>
#include <cassert>

#include "MoveOnly.h"
#include "min_allocator.h"

int main()
{
    {
        typedef int T;
        typedef std::forward_list<T> C;
        typedef std::forward_list<T> C;
        C c;
        c.push_front(1);
        c.push_front(3);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 1);
        assert(c.front() == 1);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 0);
    }
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        typedef MoveOnly T;
        typedef std::forward_list<T> C;
        C c;
        c.push_front(1);
        c.push_front(3);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 1);
        assert(c.front() == 1);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 0);
    }
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
#if __cplusplus >= 201103L
    {
        typedef int T;
        typedef std::forward_list<T, min_allocator<T>> C;
        typedef std::forward_list<T, min_allocator<T>> C;
        C c;
        c.push_front(1);
        c.push_front(3);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 1);
        assert(c.front() == 1);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 0);
    }
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        typedef MoveOnly T;
        typedef std::forward_list<T, min_allocator<T>> C;
        C c;
        c.push_front(1);
        c.push_front(3);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 1);
        assert(c.front() == 1);
        c.pop_front();
        assert(distance(c.begin(), c.end()) == 0);
    }
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
#endif
}
