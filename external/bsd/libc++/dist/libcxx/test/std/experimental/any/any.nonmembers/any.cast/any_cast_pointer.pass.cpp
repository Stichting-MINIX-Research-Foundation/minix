//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11

// <experimental/any>

// template <class ValueType>
// ValueType const* any_cast(any const *) noexcept;
//
// template <class ValueType>
// ValueType * any_cast(any *) noexcept;

#include <experimental/any>
#include <type_traits>
#include <cassert>

#include "any_helpers.h"

using std::experimental::any;
using std::experimental::any_cast;

// Test that the operators are properly noexcept.
void test_cast_is_noexcept() {
    any a;
    static_assert(noexcept(any_cast<int>(&a)), "");

    any const& ca = a;
    static_assert(noexcept(any_cast<int>(&ca)), "");
}

// Test that the return type of any_cast is correct.
void test_cast_return_type() {
    any a;
    static_assert(std::is_same<decltype(any_cast<int>(&a)), int*>::value, "");
    static_assert(std::is_same<decltype(any_cast<int const>(&a)), int const*>::value, "");

    any const& ca = a;
    static_assert(std::is_same<decltype(any_cast<int>(&ca)), int const*>::value, "");
    static_assert(std::is_same<decltype(any_cast<int const>(&ca)), int const*>::value, "");
}

// Test that any_cast handles null pointers.
void test_cast_nullptr() {
    any* a = nullptr;
    assert(nullptr == any_cast<int>(a));
    assert(nullptr == any_cast<int const>(a));

    any const* ca = nullptr;
    assert(nullptr == any_cast<int>(ca));
    assert(nullptr == any_cast<int const>(ca));
}

// Test casting an empty object.
void test_cast_empty() {
    {
        any a;
        assert(nullptr == any_cast<int>(&a));
        assert(nullptr == any_cast<int const>(&a));

        any const& ca = a;
        assert(nullptr == any_cast<int>(&ca));
        assert(nullptr == any_cast<int const>(&ca));
    }
    // Create as non-empty, then make empty and run test.
    {
        any a(42);
        a.clear();
        assert(nullptr == any_cast<int>(&a));
        assert(nullptr == any_cast<int const>(&a));

        any const& ca = a;
        assert(nullptr == any_cast<int>(&ca));
        assert(nullptr == any_cast<int const>(&ca));
    }
}

template <class Type>
void test_cast() {
    assert(Type::count == 0);
    Type::reset();
    {
        any a((Type(42)));
        any const& ca = a;
        assert(Type::count == 1);
        assert(Type::copied == 0);
        assert(Type::moved == 1);

        // Try a cast to a bad type.
        // NOTE: Type cannot be an int.
        assert(any_cast<int>(&a) == nullptr);
        assert(any_cast<int const>(&a) == nullptr);
        assert(any_cast<int const volatile>(&a) == nullptr);

        // Try a cast to the right type, but as a pointer.
        assert(any_cast<Type*>(&a) == nullptr);
        assert(any_cast<Type const*>(&a) == nullptr);

        // Check getting a unqualified type from a non-const any.
        Type* v = any_cast<Type>(&a);
        assert(v != nullptr);
        assert(v->value == 42);

        // change the stored value and later check for the new value.
        v->value = 999;

        // Check getting a const qualified type from a non-const any.
        Type const* cv = any_cast<Type const>(&a);
        assert(cv != nullptr);
        assert(cv == v);
        assert(cv->value == 999);

        // Check getting a unqualified type from a const any.
        cv = any_cast<Type>(&ca);
        assert(cv != nullptr);
        assert(cv == v);
        assert(cv->value == 999);

        // Check getting a const-qualified type from a const any.
        cv = any_cast<Type const>(&ca);
        assert(cv != nullptr);
        assert(cv == v);
        assert(cv->value == 999);

        // Check that no more objects were created, copied or moved.
        assert(Type::count == 1);
        assert(Type::copied == 0);
        assert(Type::moved == 1);
    }
    assert(Type::count == 0);
}

int main() {
    test_cast_is_noexcept();
    test_cast_return_type();
    test_cast_nullptr();
    test_cast_empty();
    test_cast<small>();
    test_cast<large>();
}
