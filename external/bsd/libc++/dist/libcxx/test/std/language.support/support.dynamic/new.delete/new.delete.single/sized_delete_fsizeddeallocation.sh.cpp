//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test sized operator delete replacement.

// Note that sized delete operator definitions below are simply ignored
// when sized deallocation is not supported, e.g., prior to C++14.

// UNSUPPORTED: sanitizer-new-delete

// NOTE: Only clang-3.7 and GCC 5.1 and greater support -fsized-deallocation.
// REQUIRES: fsized-deallocation

// RUN: %build -fsized-deallocation
// RUN: %run

#if !defined(__cpp_sized_deallocation)
# error __cpp_sized_deallocation should be defined
#endif

#if !(__cpp_sized_deallocation >= 201309L)
# error expected __cpp_sized_deallocation >= 201309L
#endif

#include <new>
#include <cstddef>
#include <cstdlib>
#include <cassert>

int unsized_delete_called = 0;
int unsized_delete_nothrow_called = 0;
int sized_delete_called = 0;

void operator delete(void* p) throw()
{
    ++unsized_delete_called;
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) throw()
{
    ++unsized_delete_nothrow_called;
    std::free(p);
}

void operator delete(void* p, std::size_t) throw()
{
    ++sized_delete_called;
    std::free(p);
}

int main()
{
    int *x = new int(42);
    assert(0 == sized_delete_called);
    assert(0 == unsized_delete_called);
    assert(0 == unsized_delete_nothrow_called);

    delete x;
    assert(1 == sized_delete_called);
    assert(0 == unsized_delete_called);
    assert(0 == unsized_delete_nothrow_called);
}
