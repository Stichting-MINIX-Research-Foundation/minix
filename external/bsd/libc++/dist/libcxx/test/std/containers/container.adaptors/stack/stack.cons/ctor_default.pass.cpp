//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <stack>

// stack();

#include <stack>
#include <vector>
#include <cassert>

#include "../../../stack_allocator.h"

int main()
{
    std::stack<int, std::vector<int, stack_allocator<int, 10> > > q;
    assert(q.size() == 0);
    q.push(1);
    q.push(2);
    assert(q.size() == 2);
    assert(q.top() == 2);
}
