//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// UNSUPPORTED: libcpp-has-no-threads

// <atomic>

// template <class T>
//     void
//     atomic_store(volatile atomic<T>* obj, T desr);
// 
// template <class T>
//     void
//     atomic_store(atomic<T>* obj, T desr);

#include <atomic>
#include <type_traits>
#include <cassert>

#include "atomic_helpers.h"

template <class T>
struct TestFn {
  void operator()() const {
    typedef std::atomic<T> A;
    A t;
    std::atomic_store(&t, T(1));
    assert(t == T(1));
    volatile A vt;
    std::atomic_store(&vt, T(2));
    assert(vt == T(2));
  }
};


int main()
{
    TestEachAtomicType<TestFn>()();
}
