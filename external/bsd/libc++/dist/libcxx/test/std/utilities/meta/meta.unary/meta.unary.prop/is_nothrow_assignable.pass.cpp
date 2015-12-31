//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// is_nothrow_assignable

#include <type_traits>

template <class T, class U>
void test_is_nothrow_assignable()
{
    static_assert(( std::is_nothrow_assignable<T, U>::value), "");
}

template <class T, class U>
void test_is_not_nothrow_assignable()
{
    static_assert((!std::is_nothrow_assignable<T, U>::value), "");
}

struct A
{
};

struct B
{
    void operator=(A);
};

struct C
{
    void operator=(C&);  // not const
};

int main()
{
    test_is_nothrow_assignable<int&, int&> ();
    test_is_nothrow_assignable<int&, int> ();
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    test_is_nothrow_assignable<int&, double> ();
#endif

    test_is_not_nothrow_assignable<int, int&> ();
    test_is_not_nothrow_assignable<int, int> ();
    test_is_not_nothrow_assignable<B, A> ();
    test_is_not_nothrow_assignable<A, B> ();
    test_is_not_nothrow_assignable<C, C&> ();
}
