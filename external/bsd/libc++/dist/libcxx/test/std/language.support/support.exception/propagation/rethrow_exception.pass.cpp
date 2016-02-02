//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <exception>

// void rethrow_exception [[noreturn]] (exception_ptr p);

#include <exception>
#include <cassert>

struct A
{
    static int constructed;
    int data_;

    A(int data = 0) : data_(data) {++constructed;}
    ~A() {--constructed;}
    A(const A& a) : data_(a.data_) {++constructed;}
};

int A::constructed = 0;

int main()
{
    {
        std::exception_ptr p;
        try
        {
            throw A(3);
        }
        catch (...)
        {
            p = std::current_exception();
        }
        try
        {
            std::rethrow_exception(p);
            assert(false);
        }
        catch (const A& a)
        {
            assert(A::constructed == 1);
            assert(p != nullptr);
            p = nullptr;
            assert(p == nullptr);
            assert(a.data_ == 3);
            assert(A::constructed == 1);
        }
        assert(A::constructed == 0);
    }
}
