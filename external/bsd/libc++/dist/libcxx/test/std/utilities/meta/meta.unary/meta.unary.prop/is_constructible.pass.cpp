//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// template <class T, class... Args>
//   struct is_constructible;

#include <type_traits>

struct A
{
    explicit A(int);
    A(int, double);
#if __has_feature(cxx_access_control_sfinae) 
private:
#endif
    A(char);
};

class Abstract
{
    virtual void foo() = 0;
};

class AbstractDestructor
{
    virtual ~AbstractDestructor() = 0;
};

template <class T>
void test_is_constructible()
{
    static_assert( (std::is_constructible<T>::value), "");
}

template <class T, class A0>
void test_is_constructible()
{
    static_assert( (std::is_constructible<T, A0>::value), "");
}

template <class T, class A0, class A1>
void test_is_constructible()
{
    static_assert( (std::is_constructible<T, A0, A1>::value), "");
}

template <class T>
void test_is_not_constructible()
{
    static_assert((!std::is_constructible<T>::value), "");
}

template <class T, class A0>
void test_is_not_constructible()
{
    static_assert((!std::is_constructible<T, A0>::value), "");
}

int main()
{
    test_is_constructible<int> ();
    test_is_constructible<int, const int> ();
    test_is_constructible<A, int> ();
    test_is_constructible<A, int, double> ();
    test_is_constructible<int&, int&> ();

    test_is_not_constructible<A> ();
#if __has_feature(cxx_access_control_sfinae) 
    test_is_not_constructible<A, char> ();
#else
    test_is_constructible<A, char> ();
#endif
    test_is_not_constructible<A, void> ();
    test_is_not_constructible<void> ();
    test_is_not_constructible<int&> ();
    test_is_not_constructible<Abstract> ();
    test_is_not_constructible<AbstractDestructor> ();
}
