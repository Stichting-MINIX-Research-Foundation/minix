//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// libc++ cannot safely provide the auto_ptr constructor without rvalue
// references.
// XFAIL: c++98, c++03

// <memory>

// unique_ptr

// template <class U> unique_ptr(auto_ptr<U>&&) noexcept

#include <memory>
#include <utility>
#include <cassert>

#include "test_macros.h"

struct A
{
    static int count;
    A() {++count;}
    A(const A&) {++count;}
    virtual ~A() {--count;}
};

int A::count = 0;

struct B
    : public A
{
    static int count;
    B() {++count;}
    B(const B&) {++count;}
    virtual ~B() {--count;}
};

int B::count = 0;

int main()
{
    {
    B* p = new B;
    std::auto_ptr<B> ap(p);
    std::unique_ptr<A> up(std::move(ap));
    assert(up.get() == p);
    assert(ap.get() == 0);
    assert(A::count == 1);
    assert(B::count == 1);
    }
    assert(A::count == 0);
    assert(B::count == 0);
    {
    B* p = new B;
    std::auto_ptr<B> ap(p);
    std::unique_ptr<A> up;
    up = std::move(ap);
    assert(up.get() == p);
    assert(ap.get() == 0);
    assert(A::count == 1);
    assert(B::count == 1);
    }
    assert(A::count == 0);
    assert(B::count == 0);
#if TEST_STD_VER >= 11
    {
        static_assert(std::is_nothrow_constructible<
            std::unique_ptr<A>,
            std::auto_ptr<B>&&
        >::value, "");
    }
#endif
}
