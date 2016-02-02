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

// shared_ptr(shared_ptr&& r);

#include <memory>
#include <cassert>

struct A
{
    static int count;

    A() {++count;}
    A(const A&) {++count;}
    ~A() {--count;}
};

int A::count = 0;

int main()
{
    {
        std::shared_ptr<A> pA(new A);
        assert(pA.use_count() == 1);
        assert(A::count == 1);
        {
            A* p = pA.get();
            std::shared_ptr<A> pA2(std::move(pA));
            assert(A::count == 1);
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
            assert(pA.use_count() == 0);
            assert(pA2.use_count() == 1);
#else  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
            assert(pA.use_count() == 2);
            assert(pA2.use_count() == 2);
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
            assert(pA2.get() == p);
        }
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
        assert(pA.use_count() == 0);
        assert(A::count == 0);
#else  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
        assert(pA.use_count() == 1);
        assert(A::count == 1);
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
    }
    assert(A::count == 0);
    {
        std::shared_ptr<A> pA;
        assert(pA.use_count() == 0);
        assert(A::count == 0);
        {
            std::shared_ptr<A> pA2(std::move(pA));
            assert(A::count == 0);
            assert(pA.use_count() == 0);
            assert(pA2.use_count() == 0);
            assert(pA2.get() == pA.get());
        }
        assert(pA.use_count() == 0);
        assert(A::count == 0);
    }
    assert(A::count == 0);
}
