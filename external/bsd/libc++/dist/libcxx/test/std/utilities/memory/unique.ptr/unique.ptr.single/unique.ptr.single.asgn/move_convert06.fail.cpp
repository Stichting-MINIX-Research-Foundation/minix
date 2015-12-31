//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <memory>

// unique_ptr

// Test unique_ptr converting move assignment

// Can't assign from const lvalue

#include <memory>
#include <utility>
#include <cassert>

#include "../../deleter.h"

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
    Deleter<B> db(5);
    const std::unique_ptr<B, Deleter<B>&> s(new B, db);
    A* p = s.get();
    Deleter<A> da(6);
    std::unique_ptr<A, Deleter<A>&> s2(new A, da);
    s2 = s;
    assert(s2.get() == p);
    assert(s.get() == 0);
    assert(A::count == 1);
    assert(B::count == 1);
    assert(s2.get_deleter().state() == 5);
    }
    assert(A::count == 0);
    assert(B::count == 0);
}
