//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <stack>

// template <class Alloc>
//   stack(const container_type& c, const Alloc& a);

#include <stack>
#include <cassert>

#include "test_allocator.h"

template <class C>
C
make(int n)
{
    C c;
    for (int i = 0; i < n; ++i)
        c.push_back(i);
    return c;
}

typedef std::deque<int, test_allocator<int> > C;

struct test
    : public std::stack<int, C>
{
    typedef std::stack<int, C> base;

    explicit test(const test_allocator<int>& a) : base(a) {}
    test(const container_type& c, const test_allocator<int>& a) : base(c, a) {}
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    test(container_type&& c, const test_allocator<int>& a) : base(std::move(c), a) {}
    test(test&& q, const test_allocator<int>& a) : base(std::move(q), a) {}
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
    test_allocator<int> get_allocator() {return c.get_allocator();}
};

int main()
{
    C d = make<C>(5);
    test q(d, test_allocator<int>(4));
    assert(q.get_allocator() == test_allocator<int>(4));
    assert(q.size() == 5);
    for (int i = 0; i < d.size(); ++i)
    {
        assert(q.top() == d[d.size() - i - 1]);
        q.pop();
    }
}
