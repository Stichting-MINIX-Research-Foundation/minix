//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <set>

// class multiset

// multiset(initializer_list<value_type> il, const key_compare& comp, const allocator_type& a);

#include <set>
#include <cassert>
#include "../../../test_compare.h"
#include "test_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
    typedef test_compare<std::less<int> > Cmp;
    typedef test_allocator<int> A;
    typedef std::multiset<int, Cmp, A> C;
    typedef C::value_type V;
    C m({1, 2, 3, 4, 5, 6}, Cmp(10), A(4));
    assert(m.size() == 6);
    assert(distance(m.begin(), m.end()) == 6);
    C::const_iterator i = m.cbegin();
    assert(*i == V(1));
    assert(*++i == V(2));
    assert(*++i == V(3));
    assert(*++i == V(4));
    assert(*++i == V(5));
    assert(*++i == V(6));
    assert(m.key_comp() == Cmp(10));
    assert(m.get_allocator() == A(4));
#endif  // _LIBCPP_HAS_NO_GENERALIZED_INITIALIZERS
}
