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

// <functional>

// template <class T>
// struct hash
//     : public unary_function<T, size_t>
// {
//     size_t operator()(T val) const;
// };

// Not very portable

#include <thread>
#include <cassert>

int main()
{
    std::thread::id id1;
    std::thread::id id2 = std::this_thread::get_id();
    typedef std::hash<std::thread::id> H;
    static_assert((std::is_same<typename H::argument_type, std::thread::id>::value), "" );
    static_assert((std::is_same<typename H::result_type, std::size_t>::value), "" );
    H h;
    assert(h(id1) != h(id2));
}
