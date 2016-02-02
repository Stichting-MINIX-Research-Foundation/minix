//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <memory>

// shared_ptr

// template<class T, class... Args> shared_ptr<T> make_shared(Args&&... args);

#include <memory>
#include <cassert>

#include "count_new.hpp"

struct A
{
    static int count;

    A(int i, char c) : int_(i), char_(c) {++count;}
    A(const A& a)
        : int_(a.int_), char_(a.char_)
        {++count;}
    ~A() {--count;}

    int get_int() const {return int_;}
    char get_char() const {return char_;}
private:
    int int_;
    char char_;
};

int A::count = 0;


struct Foo
{
    Foo() = default;
    virtual ~Foo() = default;
};


int main()
{
    int nc = globalMemCounter.outstanding_new;
    {
    int i = 67;
    char c = 'e';
    std::shared_ptr<A> p = std::make_shared<A>(i, c);
    assert(globalMemCounter.checkOutstandingNewEq(nc+1));
    assert(A::count == 1);
    assert(p->get_int() == 67);
    assert(p->get_char() == 'e');
    }

    { // https://llvm.org/bugs/show_bug.cgi?id=24137
    std::shared_ptr<Foo> p1       = std::make_shared<Foo>();
    assert(p1.get());
    std::shared_ptr<const Foo> p2 = std::make_shared<const Foo>();
    assert(p2.get());
    }

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    nc = globalMemCounter.outstanding_new;
    {
    char c = 'e';
    std::shared_ptr<A> p = std::make_shared<A>(67, c);
    assert(globalMemCounter.checkOutstandingNewEq(nc+1));
    assert(A::count == 1);
    assert(p->get_int() == 67);
    assert(p->get_char() == 'e');
    }
#endif
    assert(A::count == 0);
}
