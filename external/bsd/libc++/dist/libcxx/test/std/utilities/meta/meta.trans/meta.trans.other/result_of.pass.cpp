//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <functional>

// result_of<Fn(ArgTypes...)>

#include <type_traits>
#include <memory>
#include "test_macros.h"

struct S
{
    typedef short (*FreeFunc)(long);
    operator FreeFunc() const;
    double operator()(char, int&);
    double const& operator()(char, int&) const;
    double volatile& operator()(char, int&) volatile;
    double const volatile& operator()(char, int&) const volatile;
};

template <class Tp>
struct Voider {
    typedef void type;
};

template <class T, class = void>
struct HasType : std::false_type {};

template <class T>
struct HasType<T, typename Voider<typename T::type>::type> : std::true_type {};

template <class T, class U>
void test_result_of()
{
    static_assert((std::is_same<typename std::result_of<T>::type, U>::value), "");
}

template <class T>
void test_no_result()
{
#if TEST_STD_VER >= 11
    static_assert((!HasType<std::result_of<T> >::value), "");
#endif
}

int main()
{
    { // functor object
    test_result_of<S(int), short> ();
    test_result_of<S&(unsigned char, int&), double> ();
    test_result_of<S const&(unsigned char, int&), double const &> ();
    test_result_of<S volatile&(unsigned char, int&), double volatile&> ();
    test_result_of<S const volatile&(unsigned char, int&), double const volatile&> ();
    }
    { // pointer to function
    typedef bool         (&RF0)();
    typedef bool*       (&RF1)(int);
    typedef bool&       (&RF2)(int, int);
    typedef bool const& (&RF3)(int, int, int);
    typedef bool        (*PF0)();
    typedef bool*       (*PF1)(int);
    typedef bool&       (*PF2)(int, int);
    typedef bool const& (*PF3)(int, int, int);
    typedef bool        (*&PRF0)();
    typedef bool*       (*&PRF1)(int);
    typedef bool&       (*&PRF2)(int, int);
    typedef bool const& (*&PRF3)(int, int, int);
    test_result_of<RF0(), bool>();
    test_result_of<RF1(int), bool*>();
    test_result_of<RF2(int, long), bool&>();
    test_result_of<RF3(int, long, int), bool const&>();
    test_result_of<PF0(), bool>();
    test_result_of<PF1(int), bool*>();
    test_result_of<PF2(int, long), bool&>();
    test_result_of<PF3(int, long, int), bool const&>();
    test_result_of<PRF0(), bool>();
    test_result_of<PRF1(int), bool*>();
    test_result_of<PRF2(int, long), bool&>();
    test_result_of<PRF3(int, long, int), bool const&>();
    }
    { // pointer to member function

    typedef int         (S::*PMS0)();
    typedef int*        (S::*PMS1)(long);
    typedef int&        (S::*PMS2)(long, int);
    test_result_of<PMS0(               S),   int> ();
    test_result_of<PMS0(               S&),  int> ();
    test_result_of<PMS0(               S*),  int> ();
    test_result_of<PMS0(               S*&), int> ();
    test_result_of<PMS0(std::unique_ptr<S>), int> ();
    test_no_result<PMS0(const          S&)>();
    test_no_result<PMS0(volatile       S&)>();
    test_no_result<PMS0(const volatile S&)>();

    test_result_of<PMS1(               S,   int), int*> ();
    test_result_of<PMS1(               S&,  int), int*> ();
    test_result_of<PMS1(               S*,  int), int*> ();
    test_result_of<PMS1(               S*&, int), int*> ();
    test_result_of<PMS1(std::unique_ptr<S>, int), int*> ();
    test_no_result<PMS1(const          S&, int)>();
    test_no_result<PMS1(volatile       S&, int)>();
    test_no_result<PMS1(const volatile S&, int)>();

    test_result_of<PMS2(               S,   int, int), int&> ();
    test_result_of<PMS2(               S&,  int, int), int&> ();
    test_result_of<PMS2(               S*,  int, int), int&> ();
    test_result_of<PMS2(               S*&, int, int), int&> ();
    test_result_of<PMS2(std::unique_ptr<S>, int, int), int&> ();
    test_no_result<PMS2(const          S&, int, int)>();
    test_no_result<PMS2(volatile       S&, int, int)>();
    test_no_result<PMS2(const volatile S&, int, int)>();

    typedef int        (S::*PMS0C)() const;
    typedef int*       (S::*PMS1C)(long) const;
    typedef int&       (S::*PMS2C)(long, int) const;
    test_result_of<PMS0C(               S),   int> ();
    test_result_of<PMS0C(               S&),  int> ();
    test_result_of<PMS0C(const          S&),  int> ();
    test_result_of<PMS0C(               S*),  int> ();
    test_result_of<PMS0C(const          S*),  int> ();
    test_result_of<PMS0C(               S*&), int> ();
    test_result_of<PMS0C(const          S*&), int> ();
    test_result_of<PMS0C(std::unique_ptr<S>), int> ();
    test_no_result<PMS0C(volatile       S&)>();
    test_no_result<PMS0C(const volatile S&)>();

    test_result_of<PMS1C(               S,   int), int*> ();
    test_result_of<PMS1C(               S&,  int), int*> ();
    test_result_of<PMS1C(const          S&,  int), int*> ();
    test_result_of<PMS1C(               S*,  int), int*> ();
    test_result_of<PMS1C(const          S*,  int), int*> ();
    test_result_of<PMS1C(               S*&, int), int*> ();
    test_result_of<PMS1C(const          S*&, int), int*> ();
    test_result_of<PMS1C(std::unique_ptr<S>, int), int*> ();
    test_no_result<PMS1C(volatile       S&, int)>();
    test_no_result<PMS1C(const volatile S&, int)>();

    test_result_of<PMS2C(               S,   int, int), int&> ();
    test_result_of<PMS2C(               S&,  int, int), int&> ();
    test_result_of<PMS2C(const          S&,  int, int), int&> ();
    test_result_of<PMS2C(               S*,  int, int), int&> ();
    test_result_of<PMS2C(const          S*,  int, int), int&> ();
    test_result_of<PMS2C(               S*&, int, int), int&> ();
    test_result_of<PMS2C(const          S*&, int, int), int&> ();
    test_result_of<PMS2C(std::unique_ptr<S>, int, int), int&> ();
    test_no_result<PMS2C(volatile       S&, int, int)>();
    test_no_result<PMS2C(const volatile S&, int, int)>();

    typedef int       (S::*PMS0V)() volatile;
    typedef int*       (S::*PMS1V)(long) volatile;
    typedef int&       (S::*PMS2V)(long, int) volatile;
    test_result_of<PMS0V(               S),   int> ();
    test_result_of<PMS0V(               S&),  int> ();
    test_result_of<PMS0V(volatile       S&),  int> ();
    test_result_of<PMS0V(               S*),  int> ();
    test_result_of<PMS0V(volatile       S*),  int> ();
    test_result_of<PMS0V(               S*&), int> ();
    test_result_of<PMS0V(volatile       S*&), int> ();
    test_result_of<PMS0V(std::unique_ptr<S>), int> ();
    test_no_result<PMS0V(const          S&)>();
    test_no_result<PMS0V(const volatile S&)>();

    test_result_of<PMS1V(               S,   int), int*> ();
    test_result_of<PMS1V(               S&,  int), int*> ();
    test_result_of<PMS1V(volatile       S&,  int), int*> ();
    test_result_of<PMS1V(               S*,  int), int*> ();
    test_result_of<PMS1V(volatile       S*,  int), int*> ();
    test_result_of<PMS1V(               S*&, int), int*> ();
    test_result_of<PMS1V(volatile       S*&, int), int*> ();
    test_result_of<PMS1V(std::unique_ptr<S>, int), int*> ();
    test_no_result<PMS1V(const          S&, int)>();
    test_no_result<PMS1V(const volatile S&, int)>();

    test_result_of<PMS2V(               S,   int, int), int&> ();
    test_result_of<PMS2V(               S&,  int, int), int&> ();
    test_result_of<PMS2V(volatile       S&,  int, int), int&> ();
    test_result_of<PMS2V(               S*,  int, int), int&> ();
    test_result_of<PMS2V(volatile       S*,  int, int), int&> ();
    test_result_of<PMS2V(               S*&, int, int), int&> ();
    test_result_of<PMS2V(volatile       S*&, int, int), int&> ();
    test_result_of<PMS2V(std::unique_ptr<S>, int, int), int&> ();
    test_no_result<PMS2V(const          S&, int, int)>();
    test_no_result<PMS2V(const volatile S&, int, int)>();

    typedef int        (S::*PMS0CV)() const volatile;
    typedef int*       (S::*PMS1CV)(long) const volatile;
    typedef int&       (S::*PMS2CV)(long, int) const volatile;
    test_result_of<PMS0CV(               S),   int> ();
    test_result_of<PMS0CV(               S&),  int> ();
    test_result_of<PMS0CV(const          S&),  int> ();
    test_result_of<PMS0CV(volatile       S&),  int> ();
    test_result_of<PMS0CV(const volatile S&),  int> ();
    test_result_of<PMS0CV(               S*),  int> ();
    test_result_of<PMS0CV(const          S*),  int> ();
    test_result_of<PMS0CV(volatile       S*),  int> ();
    test_result_of<PMS0CV(const volatile S*),  int> ();
    test_result_of<PMS0CV(               S*&), int> ();
    test_result_of<PMS0CV(const          S*&), int> ();
    test_result_of<PMS0CV(volatile       S*&), int> ();
    test_result_of<PMS0CV(const volatile S*&), int> ();
    test_result_of<PMS0CV(std::unique_ptr<S>), int> ();

    test_result_of<PMS1CV(               S,   int), int*> ();
    test_result_of<PMS1CV(               S&,  int), int*> ();
    test_result_of<PMS1CV(const          S&,  int), int*> ();
    test_result_of<PMS1CV(volatile       S&,  int), int*> ();
    test_result_of<PMS1CV(const volatile S&,  int), int*> ();
    test_result_of<PMS1CV(               S*,  int), int*> ();
    test_result_of<PMS1CV(const          S*,  int), int*> ();
    test_result_of<PMS1CV(volatile       S*,  int), int*> ();
    test_result_of<PMS1CV(const volatile S*,  int), int*> ();
    test_result_of<PMS1CV(               S*&, int), int*> ();
    test_result_of<PMS1CV(const          S*&, int), int*> ();
    test_result_of<PMS1CV(volatile       S*&, int), int*> ();
    test_result_of<PMS1CV(const volatile S*&, int), int*> ();
    test_result_of<PMS1CV(std::unique_ptr<S>, int), int*> ();

    test_result_of<PMS2CV(               S,   int, int), int&> ();
    test_result_of<PMS2CV(               S&,  int, int), int&> ();
    test_result_of<PMS2CV(const          S&,  int, int), int&> ();
    test_result_of<PMS2CV(volatile       S&,  int, int), int&> ();
    test_result_of<PMS2CV(const volatile S&,  int, int), int&> ();
    test_result_of<PMS2CV(               S*,  int, int), int&> ();
    test_result_of<PMS2CV(const          S*,  int, int), int&> ();
    test_result_of<PMS2CV(volatile       S*,  int, int), int&> ();
    test_result_of<PMS2CV(const volatile S*,  int, int), int&> ();
    test_result_of<PMS2CV(               S*&, int, int), int&> ();
    test_result_of<PMS2CV(const          S*&, int, int), int&> ();
    test_result_of<PMS2CV(volatile       S*&, int, int), int&> ();
    test_result_of<PMS2CV(const volatile S*&, int, int), int&> ();
    test_result_of<PMS2CV(std::unique_ptr<S>, int, int), int&> ();
    }
    { // pointer to member data
    typedef char S::*PMD;
    test_result_of<PMD(S&), char &>();
    test_result_of<PMD(S*), char &>();
    test_result_of<PMD(S* const), char &>();
    test_result_of<PMD(const S&), const char&> ();
    test_result_of<PMD(const S*), const char&> ();
    test_result_of<PMD(volatile S&), volatile char&> ();
    test_result_of<PMD(volatile S*), volatile char&> ();
    test_result_of<PMD(const volatile S&), const volatile char&> ();
    test_result_of<PMD(const volatile S*), const volatile char&> ();
    }
}
