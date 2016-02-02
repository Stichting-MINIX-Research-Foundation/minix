//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// is_copy_assignable

#include <type_traits>

template <class T>
void test_is_copy_assignable()
{
    static_assert(( std::is_copy_assignable<T>::value), "");
}

template <class T>
void test_is_not_copy_assignable()
{
    static_assert((!std::is_copy_assignable<T>::value), "");
}

class Empty
{
};

class NotEmpty
{
public:
    virtual ~NotEmpty();
};

union Union {};

struct bit_zero
{
    int :  0;
};

struct A
{
    A();
};

class B
{
    B& operator=(const B&);
};

struct C
{
    void operator=(C&);  // not const
};

int main()
{
    test_is_copy_assignable<int> ();
    test_is_copy_assignable<int&> ();
    test_is_copy_assignable<A> ();
    test_is_copy_assignable<bit_zero> ();
    test_is_copy_assignable<Union> ();
    test_is_copy_assignable<NotEmpty> ();
    test_is_copy_assignable<Empty> ();

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    test_is_not_copy_assignable<const int> ();
    test_is_not_copy_assignable<int[]> ();
    test_is_not_copy_assignable<int[3]> ();
#endif
#if __has_feature(cxx_access_control_sfinae) 
    test_is_not_copy_assignable<B> ();
#endif
    test_is_not_copy_assignable<void> ();
    test_is_not_copy_assignable<C> ();
}
