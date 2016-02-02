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

// <shared_mutex>

// template <class Mutex> class shared_lock;

// bool try_lock();

#include <shared_mutex>
#include <cassert>

#if _LIBCPP_STD_VER > 11

bool try_lock_called = false;

struct mutex
{
    bool try_lock_shared()
    {
        try_lock_called = !try_lock_called;
        return try_lock_called;
    }
    void unlock_shared() {}
};

mutex m;

#endif  // _LIBCPP_STD_VER > 11

int main()
{
#if _LIBCPP_STD_VER > 11
    std::shared_lock<mutex> lk(m, std::defer_lock);
    assert(lk.try_lock() == true);
    assert(try_lock_called == true);
    assert(lk.owns_lock() == true);
    try
    {
        lk.try_lock();
        assert(false);
    }
    catch (std::system_error& e)
    {
        assert(e.code().value() == EDEADLK);
    }
    lk.unlock();
    assert(lk.try_lock() == false);
    assert(try_lock_called == false);
    assert(lk.owns_lock() == false);
    lk.release();
    try
    {
        lk.try_lock();
        assert(false);
    }
    catch (std::system_error& e)
    {
        assert(e.code().value() == EPERM);
    }
#endif  // _LIBCPP_STD_VER > 11
}
