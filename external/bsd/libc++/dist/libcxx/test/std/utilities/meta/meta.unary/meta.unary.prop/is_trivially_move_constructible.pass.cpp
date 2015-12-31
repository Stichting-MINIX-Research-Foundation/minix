//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// is_trivially_move_constructible

#include <type_traits>

template <class T>
void test_is_trivially_move_constructible()
{
    static_assert( std::is_trivially_move_constructible<T>::value, "");
}

template <class T>
void test_has_not_trivial_move_constructor()
{
    static_assert(!std::is_trivially_move_constructible<T>::value, "");
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

#if __has_feature(cxx_defaulted_functions)

struct MoveOnly1
{
    MoveOnly1(MoveOnly1&&);
};

struct MoveOnly2
{
    MoveOnly2(MoveOnly2&&) = default;
};

#endif

int main()
{
    test_has_not_trivial_move_constructor<void>();
    test_has_not_trivial_move_constructor<A>();
    test_has_not_trivial_move_constructor<Abstract>();
    test_has_not_trivial_move_constructor<NotEmpty>();

    test_is_trivially_move_constructible<Union>();
    test_is_trivially_move_constructible<Empty>();
    test_is_trivially_move_constructible<int>();
    test_is_trivially_move_constructible<double>();
    test_is_trivially_move_constructible<int*>();
    test_is_trivially_move_constructible<const int*>();
    test_is_trivially_move_constructible<bit_zero>();

#if __has_feature(cxx_defaulted_functions)
    static_assert(!std::is_trivially_move_constructible<MoveOnly1>::value, "");
    static_assert( std::is_trivially_move_constructible<MoveOnly2>::value, "");
#endif
}
