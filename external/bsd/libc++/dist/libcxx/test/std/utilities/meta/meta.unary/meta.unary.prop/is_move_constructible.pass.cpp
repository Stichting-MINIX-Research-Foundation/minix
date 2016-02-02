//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// is_move_constructible

#include <type_traits>

template <class T>
void test_is_move_constructible()
{
    static_assert( std::is_move_constructible<T>::value, "");
}

template <class T>
void test_is_not_move_constructible()
{
    static_assert(!std::is_move_constructible<T>::value, "");
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

class Abstract
{
public:
    virtual ~Abstract() = 0;
};

struct A
{
    A(const A&);
};

struct B
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    B(B&&);
#endif
};

int main()
{
    test_is_not_move_constructible<char[3]>();
    test_is_not_move_constructible<char[]>();
    test_is_not_move_constructible<void>();
    test_is_not_move_constructible<Abstract>();

    test_is_move_constructible<A>();
    test_is_move_constructible<int&>();
    test_is_move_constructible<Union>();
    test_is_move_constructible<Empty>();
    test_is_move_constructible<int>();
    test_is_move_constructible<double>();
    test_is_move_constructible<int*>();
    test_is_move_constructible<const int*>();
    test_is_move_constructible<NotEmpty>();
    test_is_move_constructible<bit_zero>();
    test_is_move_constructible<B>();
}
