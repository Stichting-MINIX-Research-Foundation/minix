#ifndef ATOMIC_HELPERS_H
#define ATOMIC_HELPERS_H

#include <cassert>

#include "test_macros.h"

struct UserAtomicType
{
    int i;

    explicit UserAtomicType(int d = 0) TEST_NOEXCEPT : i(d) {}

    friend bool operator==(const UserAtomicType& x, const UserAtomicType& y)
    { return x.i == y.i; }
};

template < template <class TestArg> class TestFunctor > 
struct TestEachIntegralType {
    void operator()() const {
        TestFunctor<char>()(); 
        TestFunctor<signed char>()();
        TestFunctor<unsigned char>()();
        TestFunctor<short>()();
        TestFunctor<unsigned short>()();
        TestFunctor<int>()();
        TestFunctor<unsigned int>()();
        TestFunctor<long>()();
        TestFunctor<unsigned long>()();
        TestFunctor<long long>()();
        TestFunctor<unsigned long long>()();
        TestFunctor<wchar_t>();
#ifndef _LIBCPP_HAS_NO_UNICODE_CHARS
        TestFunctor<char16_t>()();
        TestFunctor<char32_t>()();
#endif
    }
};

template < template <class TestArg> class TestFunctor > 
struct TestEachAtomicType {
    void operator()() const {
        TestEachIntegralType<TestFunctor>()();
        TestFunctor<UserAtomicType>()();
        TestFunctor<int*>()();
        TestFunctor<const int*>()();
    }
};


#endif // ATOMIC_HELPER_H
