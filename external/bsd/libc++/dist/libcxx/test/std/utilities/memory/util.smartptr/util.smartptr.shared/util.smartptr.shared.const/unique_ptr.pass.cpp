//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <memory>

// template <class Y, class D> explicit shared_ptr(unique_ptr<Y, D>&&r);

// UNSUPPORTED: sanitizer-new-delete

#include <memory>
#include <new>
#include <cstdlib>
#include <cassert>

bool throw_next = false;

void* operator new(std::size_t s) throw(std::bad_alloc)
{
    if (throw_next)
        throw std::bad_alloc();
    return std::malloc(s);
}

void  operator delete(void* p) throw()
{
    std::free(p);
}

struct B
{
    static int count;

    B() {++count;}
    B(const B&) {++count;}
    virtual ~B() {--count;}
};

int B::count = 0;

struct A
    : public B
{
    static int count;

    A() {++count;}
    A(const A&) {++count;}
    ~A() {--count;}
};

int A::count = 0;

void fn ( const std::shared_ptr<int> &) {}
void fn ( const std::shared_ptr<B> &) { assert (false); }

template <typename T>
void assert_deleter ( T * ) { assert(false); }

int main()
{
    {
    std::unique_ptr<A> ptr(new A);
    A* raw_ptr = ptr.get();
    std::shared_ptr<B> p(std::move(ptr));
    assert(A::count == 1);
    assert(B::count == 1);
    assert(p.use_count() == 1);
    assert(p.get() == raw_ptr);
    assert(ptr.get() == 0);
    }
    assert(A::count == 0);
    {
    std::unique_ptr<A> ptr(new A);
    A* raw_ptr = ptr.get();
    throw_next = true;
    try
    {
        std::shared_ptr<B> p(std::move(ptr));
        assert(false);
    }
    catch (...)
    {
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
        assert(A::count == 1);
        assert(B::count == 1);
        assert(ptr.get() == raw_ptr);
#else
        assert(A::count == 0);
        assert(B::count == 0);
        assert(ptr.get() == 0);
#endif
    }
    }
    assert(A::count == 0);

    // LWG 2399
    {
    throw_next = false;
    fn(std::unique_ptr<int>(new int));
    }

#if __cplusplus >= 201402L
    // LWG 2415
    {
    std::unique_ptr<int, void (*)(int*)> p(nullptr, assert_deleter<int>);
    std::shared_ptr<int> p2(std::move(p)); // should not call deleter when going out of scope
    }
#endif
    
}
