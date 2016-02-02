//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <vector>

// template <class InputIter> vector(InputIter first, InputIter last,
//                                   const allocator_type& a);

#include <vector>
#include <cassert>

#include "test_iterators.h"
#include "../../../stack_allocator.h"
#include "min_allocator.h"
#include "asan_testing.h"

template <class C, class Iterator, class A>
void
test(Iterator first, Iterator last, const A& a)
{
    C c(first, last, a);
    assert(c.__invariants());
    assert(c.size() == std::distance(first, last));
    assert(is_contiguous_container_asan_correct(c)); 
    for (typename C::const_iterator i = c.cbegin(), e = c.cend(); i != e; ++i, ++first)
        assert(*i == *first);
}

#if __cplusplus >= 201103L

template <class T>
struct implicit_conv_allocator : min_allocator<T>
{
    implicit_conv_allocator(void* p) {}
    implicit_conv_allocator(const implicit_conv_allocator&) = default;
};

#endif

int main()
{
    {
    int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 1, 0};
    int* an = a + sizeof(a)/sizeof(a[0]);
    std::allocator<int> alloc;
    test<std::vector<int> >(input_iterator<const int*>(a), input_iterator<const int*>(an), alloc);
    test<std::vector<int> >(forward_iterator<const int*>(a), forward_iterator<const int*>(an), alloc);
    test<std::vector<int> >(bidirectional_iterator<const int*>(a), bidirectional_iterator<const int*>(an), alloc);
    test<std::vector<int> >(random_access_iterator<const int*>(a), random_access_iterator<const int*>(an), alloc);
    test<std::vector<int> >(a, an, alloc);
    }
#if __cplusplus >= 201103L
    {
    int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 1, 0};
    int* an = a + sizeof(a)/sizeof(a[0]);
    min_allocator<int> alloc;
    test<std::vector<int, min_allocator<int>> >(input_iterator<const int*>(a), input_iterator<const int*>(an), alloc);
    test<std::vector<int, min_allocator<int>> >(forward_iterator<const int*>(a), forward_iterator<const int*>(an), alloc);
    test<std::vector<int, min_allocator<int>> >(bidirectional_iterator<const int*>(a), bidirectional_iterator<const int*>(an), alloc);
    test<std::vector<int, min_allocator<int>> >(random_access_iterator<const int*>(a), random_access_iterator<const int*>(an), alloc);
    test<std::vector<int, min_allocator<int>> >(a, an, alloc);
    test<std::vector<int, implicit_conv_allocator<int>> >(a, an, nullptr);
    }
#endif
}
